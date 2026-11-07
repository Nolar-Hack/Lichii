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
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QProgressBar>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStringList>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

#include "common/common_types.h"
#include "common/fs/path_util.h"
#include "common/scope_exit.h"
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
    QString tab;
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
        View view{object[QStringLiteral("name")].toString(),
                  object[QStringLiteral("tab")].toString(),
                  {},
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
        "QLabel { color: #e9e6da; font-size: 12px; background: transparent; }"
        "QLabel#header { color: #f4d27a; font-size: 19px; font-weight: 600; }"
        "QLabel#subtitle { color: #8f9a86; font-size: 10px; letter-spacing: 1px; }"
        "QLabel#value { color: #ffffff; font-weight: 600; }"
        "QComboBox { color: #f1efe6; background: rgba(255,255,255,16); border: 1px solid "
        "rgba(255,255,255,26); border-radius: 7px; padding: 4px 9px; font-size: 12px; }"
        "QComboBox::drop-down { border: none; width: 16px; }"
        "QComboBox QAbstractItemView { color: #f1efe6; background: #171b17; border: 1px solid "
        "#3a4436; selection-background-color: #4c6a35; outline: none; }"
        "QTabBar { background: transparent; }"
        "QTabBar::tab { color: #9aa390; background: transparent; padding: 3px 8px; border: none; "
        "font-size: 11px; }"
        "QTabBar::tab:selected { color: #ffffff; border-bottom: 2px solid #9fd36a; }"
        "QProgressBar { background: rgba(255,255,255,20); border: none; border-radius: 2px; "
        "max-height: 4px; min-height: 4px; }"
        "QProgressBar::chunk { background: #9fd36a; border-radius: 2px; }"
        "QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }"
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
        "QScrollBar::handle:vertical { background: rgba(255,255,255,50); border-radius: 3px; "
        "min-height: 20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QToolButton { color: #aeb5a4; background: transparent; border: none; font-size: 15px; "
        "padding: 0 4px; }"
        "QToolButton:hover { color: #ffffff; }"));

    subtitle = new QLabel(this);
    subtitle->setObjectName(QStringLiteral("subtitle"));
    subtitle->installEventFilter(this);
    header = new QLabel(this);
    header->setObjectName(QStringLiteral("header"));
    header->installEventFilter(this);
    collapse = new QToolButton(this);
    collapse->setText(QStringLiteral("–"));
    auto* titles = new QVBoxLayout;
    titles->setContentsMargins(0, 0, 0, 0);
    titles->setSpacing(0);
    titles->addWidget(subtitle);
    titles->addWidget(header);
    auto* top = new QHBoxLayout;
    top->setContentsMargins(0, 0, 0, 0);
    top->addLayout(titles, 1);
    top->addWidget(collapse, 0, Qt::AlignTop);

    tabs = new QTabBar(this);
    tabs->setDrawBase(false);
    tabs->setExpanding(false);
    tabs->addTab(QStringLiteral("Global"));
    if (game) {
        QStringList names;
        for (const View& view : game->views) {
            if (!names.contains(view.tab)) {
                names.push_back(view.tab);
                tabs->addTab(view.tab);
            }
        }
    }
    views = new QComboBox(this);

    body = new QWidget;
    rows = new QGridLayout(body);
    rows->setContentsMargins(2, 4, 8, 2);
    rows->setHorizontalSpacing(12);
    rows->setVerticalSpacing(5);
    rows->setColumnStretch(0, 1);
    scroll = new QScrollArea(this);
    scroll->setWidget(body);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 11, 10, 12);
    layout->setSpacing(7);
    layout->addLayout(top);
    layout->addWidget(tabs);
    layout->addWidget(views);
    layout->addWidget(scroll, 1);
    grip = new QLabel(QStringLiteral("⌟"), this);
    grip->setObjectName(QStringLiteral("subtitle"));
    grip->setCursor(Qt::SizeFDiagCursor);
    grip->installEventFilter(this);
    layout->addWidget(grip, 0, Qt::AlignRight);
    setMinimumWidth(230);

    connect(tabs, &QTabBar::currentChanged, this, &AdventureNotes::FillViews);
    connect(views, &QComboBox::currentIndexChanged, this, &AdventureNotes::Refresh);
    connect(collapse, &QToolButton::clicked, this, [this] {
        const bool visible = !scroll->isVisible();
        scroll->setVisible(visible);
        tabs->setVisible(visible);
        views->setVisible(visible && views->count() > 1);
        grip->setVisible(visible);
        collapse->setText(visible ? QStringLiteral("–") : QStringLiteral("+"));
        sized_by_hand = false;
        FitContent();
    });
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &AdventureNotes::Refresh);
    timer->start(3000);
    FillViews();
}

void AdventureNotes::FillViews() {
    const QSignalBlocker blocker{views};
    views->clear();
    if (game && tabs->currentIndex() > 0) {
        const QString tab = tabs->tabText(tabs->currentIndex());
        for (size_t index = 0; index < game->views.size(); ++index) {
            if (game->views[index].tab == tab) {
                views->addItem(game->views[index].name, static_cast<int>(index));
            }
        }
    }
    views->setVisible(views->count() > 1);
    Refresh();
}

void AdventureNotes::FitContent() {
    if (sized_by_hand) {
        return;
    }
    // As tall as what it shows, up to a limit: a short view stays small.
    constexpr int MaxListHeight = 330;
    rows->activate();
    const int wanted = std::min(body->sizeHint().height(), MaxListHeight);
    scroll->setMinimumHeight(wanted);
    scroll->setMaximumHeight(wanted);
    adjustSize();
}

void AdventureNotes::Refresh() {
    SCOPE_EXIT {
        FitContent();
    };
    while (QLayoutItem* item = rows->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    const auto path = game ? NewestSave(*game) : std::nullopt;
    if (!path) {
        header->setText(tr("No save found"));
        return;
    }
    const SaveTable save{*path};
    if (!save.Valid()) {
        header->setText(tr("Unreadable save"));
        return;
    }
    const s64 seconds = save.Read(game->play_time_hash, FieldKind::Count);
    subtitle->setText(QStringLiteral("%1  ·  %2")
                          .arg(game->title.toUpper().left(26))
                          .arg(QString::fromStdString(path->parent_path().filename().string())));
    header->setText(QStringLiteral("%1 h %2")
                        .arg(seconds / 3600)
                        .arg((seconds % 3600) / 60, 2, 10, QLatin1Char('0')));
    int row = 0;
    const auto add = [this, &row](const QString& title, const QString& value) {
        auto* name = new QLabel(title, body);
        name->setWordWrap(true);
        auto* text = new QLabel(value, body);
        text->setObjectName(QStringLiteral("value"));
        text->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rows->addWidget(name, row, 0);
        rows->addWidget(text, row, 1);
        ++row;
    };
    const auto progress = [this, &row, &add](const QString& title, int done, int total) {
        add(title, QStringLiteral("%1 / %2").arg(done).arg(total));
        auto* bar = new QProgressBar(body);
        bar->setRange(0, std::max(total, 1));
        bar->setValue(done);
        bar->setTextVisible(false);
        rows->addWidget(bar, row, 0, 1, 2);
        ++row;
    };
    const int selected = views->count() > 0 ? views->currentData().toInt() : -1;
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
            progress(view.name, done, total);
        }
        for (const Collection& collection : game->collections) {
            int found = 0, total = 0;
            for (const Zone& zone : collection.zones) {
                found += save.CountKeys(zone.keys);
                total += static_cast<int>(zone.keys.size());
            }
            progress(collection.name, found, total);
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
            add(summary.title, QString::number(value));
        }
        return;
    }
    const View& view = game->views[static_cast<size_t>(selected)];
    if (view.collection >= 0) {
        for (const Zone& zone : game->collections[static_cast<size_t>(view.collection)].zones) {
            const int found = save.CountKeys(zone.keys);
            const int total = static_cast<int>(zone.keys.size());
            progress(zone.name, found, total);
        }
        return;
    }
    for (const Field& field : view.fields) {
        const s64 value = save.Read(field.hash, field.kind);
        const QString shown =
            field.kind == FieldKind::Flag
                ? (value != 0 ? QStringLiteral("<span style='color:#9fd36a'>✓</span>")
                              : QStringLiteral("<span style='color:#6d7566'>—</span>"))
                : QString::number(value);
        add(field.title, shown);
    }
    for (const Collection& collection : game->collections) {
        for (const Zone& zone : collection.zones) {
            if (zone.name == view.name) {
                progress(collection.name, save.CountKeys(zone.keys),
                         static_cast<int>(zone.keys.size()));
            }
        }
    }
}

void AdventureNotes::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QLinearGradient gradient{0, 0, 0, static_cast<qreal>(height())};
    gradient.setColorAt(0.0, QColor{30, 36, 28, 228});
    gradient.setColorAt(1.0, QColor{14, 17, 14, 228});
    painter.setBrush(gradient);
    painter.setPen(QPen{QColor{255, 255, 255, 26}, 1.0});
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 12.0, 12.0);
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
    if (object == grip && event->type() == QEvent::MouseButtonPress) {
        // By hand from now on: the list takes what the window gives it.
        sized_by_hand = true;
        scroll->setMinimumHeight(0);
        scroll->setMaximumHeight(QWIDGETSIZE_MAX);
        if (windowHandle()) {
            windowHandle()->startSystemResize(Qt::RightEdge | Qt::BottomEdge);
        }
        return true;
    }
    if ((object == header || object == subtitle) && event->type() == QEvent::MouseButtonPress) {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton) {
            StartDrag(static_cast<QWidget*>(object)->mapTo(this, mouse->pos()));
            return true;
        }
    }
    return QWidget::eventFilter(object, event);
}

void AdventureNotes::closeEvent(QCloseEvent*) {
    emit closed();
}
