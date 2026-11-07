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
#include <vector>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QTabBar>
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

// The tabs a record appears in.
enum Tab : u32 { Summary = 1, Combat = 2, Exploration = 4, Actions = 8 };

struct Record {
    u32 hash;
    const char* title;
    u32 tabs;
};

// The game's Int64 save keys. Names from the public demo of https://onif.info/ocarina/.
constexpr std::array<Record, 23> OcarinaRecords{{
    {0x8bc1257a, "Morts", Summary | Combat},
    {0xf7cb0ee2, "Ennemis vaincus", Summary | Combat},
    {0x15ea6aa2, "Skulltulas d'or", Summary | Exploration},
    {0x4766a777, "Coffres", Summary | Exploration},
    {0x0b7f5726, "Rubis ramassés", Summary},
    {0xcb183657, "Pots cassés", Actions},
    {0x824b47b1, "Herbe coupée", Actions},
    {0x30bbe1d8, "Pierres soulevées", Exploration},
    {0x2a24450a, "Coups d'épée", Combat},
    {0x4ac3ff5d, "Roulades", Actions},
    {0xa6454431, "Sauts", Actions},
    {0x3eeafadd, "Distance parcourue", Exploration},
    {0x392c7b86, "Temps sous l'eau", Exploration},
    {0xe8c057bc, "Vol de cocotte", Exploration},
    {0x1957b8c5, "Objets utilisés", Combat},
    {0xf5f1954d, "Magie dépensée", Combat},
    {0xd457ee3a, "Ocarina joué", Actions},
    {0x64b98a4e, "Discussions", Actions},
    {0xc0be46bc, "Discussions avec Navi", Actions},
    {0x70d7014b, "Pierres à potins", Exploration},
    {0x3f31ad53, "Mini-jeux", Actions},
    {0xcaa00e60, "Pêche", Actions},
    {0xfd680ada, "Amiibo", Actions},
}};
constexpr u32 OcarinaPlayTime = 0xce97845e;

} // Anonymous namespace

/// A game whose save the panel can read: hash + value tables at 0x28, as in progress.sav.
struct GameNotes {
    u64 program_id;
    const char* title;
    const char* folder;    ///< Its folder in the save directory.
    const char* save_file; ///< The file in each of its slot folders.
    u32 play_time_hash;
    std::span<const Record> records;
};

namespace {
constexpr std::array<GameNotes, 1> Games{{
    {0x0400486024DEC000ULL, "The Legend of Zelda: Ocarina of Time", "0400486024DEC000",
     "progress.sav", OcarinaPlayTime, OcarinaRecords},
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

std::optional<std::unordered_map<u32, s64>> ReadRecords(const GameNotes& game,
                                                        const fs::path& path) {
    std::ifstream file{path, std::ios::binary};
    std::vector<char> raw{std::istreambuf_iterator<char>{file}, {}};
    if (raw.size() < 0x30) {
        return std::nullopt;
    }
    const auto u32_at = [&raw](size_t offset) {
        u32 value;
        std::memcpy(&value, raw.data() + offset, sizeof(value));
        return value;
    };
    std::unordered_map<u32, u32> table;
    const size_t end = u32_at(8);
    for (size_t pos = 0x28; pos + 8 <= raw.size() && pos < end; pos += 8) {
        table.try_emplace(u32_at(pos), u32_at(pos + 4));
    }
    std::unordered_map<u32, s64> records;
    const auto read = [&](u32 hash) {
        const auto it = table.find(hash);
        if (it != table.end() && size_t{it->second} + sizeof(s64) <= raw.size()) {
            s64 value;
            std::memcpy(&value, raw.data() + it->second, sizeof(value));
            records.emplace(hash, value);
        }
    };
    read(game.play_time_hash);
    for (const Record& record : game.records) {
        read(record.hash);
    }
    return records;
}
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
        "QTabBar { background: transparent; }"
        "QTabBar::tab { color: #a8a8a0; background: transparent; padding: 2px 7px; border: none; "
        "font-size: 10px; }"
        "QTabBar::tab:selected { color: #ffffff; border-bottom: 1px solid #9fd36a; }"
        "QToolButton { color: #c8c8c0; background: transparent; border: none; font-size: 12px; }"));

    header = new QLabel(this);
    header->installEventFilter(this);
    collapse = new QToolButton(this);
    collapse->setText(QStringLiteral("–"));
    auto* top = new QHBoxLayout;
    top->setContentsMargins(0, 0, 0, 0);
    top->addWidget(header, 1);
    top->addWidget(collapse);

    tabs = new QTabBar(this);
    tabs->setDrawBase(false);
    tabs->setExpanding(false);
    for (const char* name : {"Résumé", "Combat", "Exploration", "Actions"}) {
        tabs->addTab(QString::fromUtf8(name));
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
    layout->addWidget(tabs);
    layout->addWidget(body);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    connect(tabs, &QTabBar::currentChanged, this, &AdventureNotes::Refresh);
    connect(collapse, &QToolButton::clicked, this, [this] {
        const bool visible = !body->isVisible();
        body->setVisible(visible);
        tabs->setVisible(visible);
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
    const auto records = path ? ReadRecords(*game, *path) : std::nullopt;
    if (!records) {
        header->setText(tr("Aucune sauvegarde trouvée"));
        return;
    }
    const auto find = [&records](u32 hash) {
        const auto it = records->find(hash);
        return it != records->end() ? it->second : s64{0};
    };
    const s64 seconds = find(game->play_time_hash);
    header->setText(QStringLiteral("<b>%1 h %2</b> &nbsp;<span style='color:#a8a8a0'>%3</span>")
                        .arg(seconds / 3600)
                        .arg((seconds % 3600) / 60, 2, 10, QLatin1Char('0'))
                        .arg(QString::fromStdString(path->parent_path().filename().string())));
    const u32 tab = 1U << std::max(tabs->currentIndex(), 0);
    int row = 0;
    for (const Record& record : game->records) {
        if ((record.tabs & tab) == 0) {
            continue;
        }
        auto* name = new QLabel(QString::fromUtf8(record.title), body);
        auto* value = new QLabel(QStringLiteral("<b>%1</b>").arg(find(record.hash)), body);
        value->setAlignment(Qt::AlignRight);
        rows->addWidget(name, row, 0);
        rows->addWidget(value, row, 1);
        ++row;
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
