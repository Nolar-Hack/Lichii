// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
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
    const char* title;
    FieldKind kind;
};

struct View {
    const char* name;
    std::span<const Field> fields;
    bool checklist;      ///< Counted in the Global view.
    int collection = -1; ///< The Collection this view lists by zone, if any.
};

/// A zone's collectibles, kept in the save as 64-bit actor keys once taken.
struct Zone {
    const char* name;
    std::span<const u64> tokens;
};

/// Collectibles of one kind, zone by zone.
struct Collection {
    const char* name;
    std::span<const Zone> zones;
};

#include "render/adventure_notes_ocarina.inc"

constexpr u32 OcarinaPlayTime = 0xce97845e;
} // Anonymous namespace

/// A game whose save the panel can read: hash + value tables at 0x28, as in progress.sav.
struct GameNotes {
    u64 program_id;
    const char* title;
    const char* folder;    ///< Its folder in the save directory.
    const char* save_file; ///< The file in each of its slot folders.
    u32 play_time_hash;
    std::span<const View> views;
    std::span<const Collection> collections;
};

namespace {
constexpr std::array<GameNotes, 1> Games{{
    {0x0400486024DEC000ULL, "The Legend of Zelda: Ocarina of Time", "0400486024DEC000",
     "progress.sav", OcarinaPlayTime, OcarinaViews, OcarinaCollections},
}};

const GameNotes* FindGame(u64 program_id) {
    for (const GameNotes& game : Games) {
        if (game.program_id == program_id) {
            return &game;
        }
    }
    return nullptr;
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

    [[nodiscard]] int CountKeys(std::span<const u64> wanted) const {
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
    return FindGame(program_id) != nullptr;
}

AdventureNotes::AdventureNotes(MainWindow* parent, u64 program_id)
    : QWidget(parent), game{FindGame(program_id)} {
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
            views->addItem(QString::fromUtf8(view.name));
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
            add(QString::fromUtf8(view.name), QStringLiteral("<b>%1 / %2</b>").arg(done).arg(total));
        }
        for (const Collection& collection : game->collections) {
            int found = 0, total = 0;
            for (const Zone& zone : collection.zones) {
                found += save.CountKeys(zone.tokens);
                total += static_cast<int>(zone.tokens.size());
            }
            add(QString::fromUtf8(collection.name),
                QStringLiteral("<b>%1 / %2</b>").arg(found).arg(total));
        }
        return;
    }
    const View& view = game->views[static_cast<size_t>(selected)];
    if (view.collection >= 0) {
        for (const Zone& zone : game->collections[static_cast<size_t>(view.collection)].zones) {
            const int found = save.CountKeys(zone.tokens);
            const int total = static_cast<int>(zone.tokens.size());
            const char* const color = found == total ? "#9fd36a" : "#ffffff";
            add(QString::fromUtf8(zone.name),
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
        add(QString::fromUtf8(field.title), shown);
    }
    for (const Collection& collection : game->collections) {
        for (const Zone& zone : collection.zones) {
            if (std::strcmp(zone.name, view.name) == 0) {
                add(QString::fromUtf8(collection.name), QStringLiteral("<b>%1 / %2</b>")
                                                            .arg(save.CountKeys(zone.tokens))
                                                            .arg(zone.tokens.size()));
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
