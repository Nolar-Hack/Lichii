// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <fmt/format.h>

#include <QComboBox>
#include <QFile>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

#include "common/common_types.h"
#include "common/fs/path_util.h"
#include "main_window.h"
#include "render/adventure_notes.h"

namespace {
namespace fs = std::filesystem;

enum class FieldKind : u8 {
    Count,  ///< 64-bit value at the offset the table holds.
    Flag,   ///< Boolean held in the table.
    Number, ///< 32-bit value held in the table.
};

struct Field {
    u32 hash;
    QString title;
    FieldKind kind;
};

struct View {
    QString name;
    std::vector<Field> fields;
    bool checklist;
    int collection = -1; ///< The collection this view lists by zone, if any.
};

/// A zone's collectibles, kept in the save as 64-bit actor keys once taken.
struct Zone {
    QString name;
    std::vector<u64> keys;
};

struct Collection {
    QString name;
    std::vector<Zone> zones;
};

/// One term of a Global line: a table value, a collection's count or a constant, times a scale.
struct Term {
    u32 hash = 0;
    int collection = -1;
    s64 constant = 0;
    s64 scale = 1;
};

struct Summary {
    QString title;
    std::vector<Term> terms;
};
} // Anonymous namespace

/// A module: what to read in a game's save. The only layout so far is "gamedata-table".
struct GameNotes {
    QString title;
    std::string folder;
    std::string save_file;
    u32 play_time_hash = 0;
    std::vector<View> views;
    std::vector<Collection> collections;
    std::vector<Summary> summaries;
};

namespace {
fs::path ModulePath(u64 program_id) {
    return Common::FS::GetEdenPath(Common::FS::EdenPath::EdenDir) / "notes" /
           fmt::format("{:016X}.json", program_id);
}

u64 ParseHex(const QJsonValue& value) {
    return value.toString().toULongLong(nullptr, 16);
}

FieldKind ParseKind(const QString& kind) {
    if (kind == QStringLiteral("flag")) {
        return FieldKind::Flag;
    }
    return kind == QStringLiteral("number") ? FieldKind::Number : FieldKind::Count;
}

std::unique_ptr<GameNotes> LoadModule(u64 program_id) {
    QFile file{QString::fromStdString(ModulePath(program_id).string())};
    if (!file.open(QIODevice::ReadOnly)) {
        return nullptr;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonObject save = root[QStringLiteral("save")].toObject();
    if (save[QStringLiteral("layout")].toString() != QStringLiteral("gamedata-table")) {
        return nullptr;
    }
    auto game = std::make_unique<GameNotes>();
    game->title = root[QStringLiteral("title")].toString();
    game->folder = save[QStringLiteral("folder")].toString().toStdString();
    game->save_file = save[QStringLiteral("file")].toString().toStdString();
    game->play_time_hash = static_cast<u32>(ParseHex(root[QStringLiteral("play_time")]));
    for (const QJsonValue& entry : root[QStringLiteral("views")].toArray()) {
        const QJsonObject object = entry.toObject();
        View view{object[QStringLiteral("name")].toString(), {},
                  object[QStringLiteral("checklist")].toBool(),
                  object[QStringLiteral("collection")].toInt(-1)};
        for (const QJsonValue& item : object[QStringLiteral("fields")].toArray()) {
            const QJsonObject field = item.toObject();
            view.fields.push_back({static_cast<u32>(ParseHex(field[QStringLiteral("hash")])),
                                   field[QStringLiteral("title")].toString(),
                                   ParseKind(field[QStringLiteral("kind")].toString())});
        }
        game->views.push_back(std::move(view));
    }
    for (const QJsonValue& entry : root[QStringLiteral("collections")].toArray()) {
        const QJsonObject object = entry.toObject();
        Collection collection{object[QStringLiteral("name")].toString(), {}};
        for (const QJsonValue& item : object[QStringLiteral("zones")].toArray()) {
            const QJsonObject zone_object = item.toObject();
            Zone zone{zone_object[QStringLiteral("name")].toString(), {}};
            for (const QJsonValue& key : zone_object[QStringLiteral("keys")].toArray()) {
                zone.keys.push_back(ParseHex(key));
            }
            collection.zones.push_back(std::move(zone));
        }
        game->collections.push_back(std::move(collection));
    }
    for (const QJsonValue& entry : root[QStringLiteral("summary")].toArray()) {
        const QJsonObject object = entry.toObject();
        Summary summary{object[QStringLiteral("title")].toString(), {}};
        for (const QJsonValue& item : object[QStringLiteral("terms")].toArray()) {
            const QJsonObject term = item.toObject();
            summary.terms.push_back({static_cast<u32>(ParseHex(term[QStringLiteral("hash")])),
                                     term[QStringLiteral("collection")].toInt(-1),
                                     term[QStringLiteral("constant")].toInteger(),
                                     term[QStringLiteral("scale")].toInteger(1)});
        }
        game->summaries.push_back(std::move(summary));
    }
    return game;
}

std::optional<fs::path> NewestSave(const GameNotes& game) {
    std::error_code ec;
    const fs::path saves = Common::FS::GetEdenPath(Common::FS::EdenPath::NANDDir) / "user" / "save" /
                           "0000000000000000";
    std::optional<fs::path> newest;
    fs::file_time_type newest_time{};
    for (const auto& profile : fs::directory_iterator{saves, ec}) {
        const fs::path title = profile.path() / game.folder;
        for (const auto& slot : fs::directory_iterator{title, ec}) {
            const fs::path file = slot.path() / game.save_file;
            const auto time = fs::last_write_time(file, ec);
            if (!ec && (!newest || time > newest_time)) {
                newest = file;
                newest_time = time;
            }
            ec.clear();
        }
    }
    return newest;
}

class SaveTable {
public:
    explicit SaveTable(const fs::path& path) {
        std::ifstream file{path, std::ios::binary};
        raw.assign(std::istreambuf_iterator<char>{file}, {});
        if (raw.size() < 0x30) {
            return;
        }
        const size_t end = Word(8);
        for (size_t pos = 0x28; pos + 8 <= raw.size() && pos < end; pos += 8) {
            table.try_emplace(Word(pos), Word(pos + 4));
        }
        for (size_t pos = end & ~size_t{7}; pos + 8 <= raw.size(); pos += 8) {
            u64 key;
            std::memcpy(&key, raw.data() + pos, sizeof(key));
            keys.insert(key);
        }
    }

    [[nodiscard]] int CountKeys(const std::vector<u64>& wanted) const {
        return static_cast<int>(std::ranges::count_if(
            wanted, [this](u64 key) { return keys.contains(key); }));
    }

    [[nodiscard]] bool Valid() const {
        return !table.empty();
    }

    [[nodiscard]] s64 Read(u32 hash, FieldKind kind) const {
        const auto it = table.find(hash);
        if (it == table.end()) {
            return 0;
        }
        if (kind != FieldKind::Count) {
            return it->second;
        }
        s64 value{};
        if (size_t{it->second} + sizeof(value) <= raw.size()) {
            std::memcpy(&value, raw.data() + it->second, sizeof(value));
        }
        return value;
    }

private:
    [[nodiscard]] u32 Word(size_t offset) const {
        u32 value;
        std::memcpy(&value, raw.data() + offset, sizeof(value));
        return value;
    }

    std::vector<char> raw;
    std::unordered_map<u32, u32> table;
    std::unordered_set<u64> keys;
};
} // Anonymous namespace

bool AdventureNotes::Supports(u64 program_id) {
    std::error_code ec;
    return fs::exists(ModulePath(program_id), ec);
}

AdventureNotes::~AdventureNotes() = default;

AdventureNotes::AdventureNotes(MainWindow* parent, u64 program_id)
    : QWidget(parent), game{LoadModule(program_id)} {
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setStyleSheet(QStringLiteral(
        "QLabel { color: #e8e8e0; font-size: 11px; background: transparent; }"
        "QComboBox { color: #ffffff; background: rgba(255,255,255,18); border: none; "
        "border-radius: 4px; padding: 1px 6px; font-size: 11px; }"
        "QComboBox QAbstractItemView { color: #ffffff; background: #1c201c; "
        "selection-background-color: #3d5a2a; }"
        "QToolButton { color: #c8c8c0; background: transparent; border: none; font-size: 12px; }"));

    header = new QLabel(this);
    header->installEventFilter(this);
    collapse = new QToolButton(this);
    collapse->setText(QStringLiteral("–"));
    auto* top = new QHBoxLayout;
    top->setContentsMargins(0, 0, 0, 0);
    top->addWidget(header, 1);
    top->addWidget(collapse);

    views = new QComboBox(this);
    views->addItem(QStringLiteral("Global"));
    if (game) {
        for (const View& view : game->views) {
            views->addItem(view.name);
        }
    }

    body = new QWidget(this);
    rows = new QGridLayout(body);
    rows->setContentsMargins(4, 2, 4, 0);
    rows->setHorizontalSpacing(14);
    rows->setVerticalSpacing(1);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 6, 8, 8);
    layout->setSpacing(3);
    layout->addLayout(top);
    layout->addWidget(views);
    layout->addWidget(body);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    connect(views, &QComboBox::currentIndexChanged, this, &AdventureNotes::Refresh);
    connect(collapse, &QToolButton::clicked, this, [this] {
        const bool visible = !body->isVisible();
        body->setVisible(visible);
        views->setVisible(visible);
        collapse->setText(visible ? QStringLiteral("–") : QStringLiteral("+"));
    });
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &AdventureNotes::Refresh);
    timer->start(3000);
    Refresh();
}

void AdventureNotes::Refresh() {
    while (QLayoutItem* item = rows->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    const auto path = game ? NewestSave(*game) : std::nullopt;
    if (!path) {
        header->setText(tr("Aucune sauvegarde trouvée"));
        return;
    }
    const SaveTable save{*path};
    if (!save.Valid()) {
        header->setText(tr("Sauvegarde illisible"));
        return;
    }
    const s64 seconds = save.Read(game->play_time_hash, FieldKind::Count);
    header->setText(QStringLiteral("<b>%1 h %2</b> &nbsp;<span style='color:#a8a8a0'>%3</span>")
                        .arg(seconds / 3600)
                        .arg((seconds % 3600) / 60, 2, 10, QLatin1Char('0'))
                        .arg(QString::fromStdString(path->parent_path().filename().string())));
    int row = 0;
    const auto add = [this, &row](const QString& title, const QString& value) {
        auto* name = new QLabel(title, body);
        auto* text = new QLabel(value, body);
        text->setAlignment(Qt::AlignRight);
        rows->addWidget(name, row, 0);
        rows->addWidget(text, row, 1);
        ++row;
    };
    const int selected = views->currentIndex() - 1;
    if (selected < 0) {
        // Global: what each checklist holds, to see where something is missing.
        for (const View& view : game->views) {
            if (!view.checklist) {
                continue;
            }
            int done = 0, total = 0;
            for (const Field& field : view.fields) {
                if (field.kind == FieldKind::Flag) {
                    ++total;
                    done += save.Read(field.hash, field.kind) != 0 ? 1 : 0;
                }
            }
            add(view.name, QStringLiteral("<b>%1 / %2</b>").arg(done).arg(total));
        }
        for (const Collection& collection : game->collections) {
            int found = 0, total = 0;
            for (const Zone& zone : collection.zones) {
                found += save.CountKeys(zone.keys);
                total += static_cast<int>(zone.keys.size());
            }
            add(collection.name,
                QStringLiteral("<b>%1 / %2</b>").arg(found).arg(total));
        }
        for (const Summary& summary : game->summaries) {
            s64 value = 0;
            for (const Term& term : summary.terms) {
                s64 part = term.constant;
                if (term.hash != 0) {
                    part = save.Read(term.hash, FieldKind::Number);
                } else if (term.collection >= 0) {
                    part = 0;
                    for (const Zone& zone :
                         game->collections[static_cast<size_t>(term.collection)].zones) {
                        part += save.CountKeys(zone.keys);
                    }
                }
                value += part * (term.hash != 0 || term.collection >= 0 ? term.scale : 1);
            }
            add(summary.title, QStringLiteral("<b>%1</b>").arg(value));
        }
        return;
    }
    const View& view = game->views[static_cast<size_t>(selected)];
    if (view.collection >= 0) {
        for (const Zone& zone : game->collections[static_cast<size_t>(view.collection)].zones) {
            const int found = save.CountKeys(zone.keys);
            const int total = static_cast<int>(zone.keys.size());
            const char* const color = found == total ? "#9fd36a" : "#ffffff";
            add(zone.name,
                QStringLiteral("<b style='color:%1'>%2 / %3</b>")
                    .arg(QString::fromLatin1(color))
                    .arg(found)
                    .arg(total));
        }
        return;
    }
    for (const Field& field : view.fields) {
        const s64 value = save.Read(field.hash, field.kind);
        const QString shown = field.kind == FieldKind::Flag
                                  ? (value != 0 ? QStringLiteral("<b style='color:#9fd36a'>oui</b>")
                                                : QStringLiteral("<span style='color:#d08a6a'>non</span>"))
                                  : QStringLiteral("<b>%1</b>").arg(value);
        add(field.title, shown);
    }
    for (const Collection& collection : game->collections) {
        for (const Zone& zone : collection.zones) {
            if (zone.name == view.name) {
                add(collection.name, QStringLiteral("<b>%1 / %2</b>")
                                                            .arg(save.CountKeys(zone.keys))
                                                            .arg(zone.keys.size()));
            }
        }
    }
}

void AdventureNotes::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor{16, 18, 16, 170});
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), 8.0, 8.0);
}

void AdventureNotes::StartDrag(QPoint position) {
    // The compositor moves the window where it can: Wayland ignores move() on top-level windows.
    if (windowHandle() && windowHandle()->startSystemMove()) {
        return;
    }
    dragging = true;
    drag_start = position;
}

void AdventureNotes::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        StartDrag(event->pos());
    }
}

void AdventureNotes::mouseMoveEvent(QMouseEvent* event) {
    if (dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - drag_start);
    }
}

bool AdventureNotes::eventFilter(QObject* object, QEvent* event) {
    if (object == header && event->type() == QEvent::MouseButtonPress) {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton) {
            StartDrag(header->mapTo(this, mouse->pos()));
            return true;
        }
    }
    return QWidget::eventFilter(object, event);
}

void AdventureNotes::closeEvent(QCloseEvent*) {
    emit closed();
}
