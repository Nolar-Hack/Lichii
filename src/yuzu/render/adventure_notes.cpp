// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include <QGuiApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

#include "common/common_types.h"
#include "common/fs/path_util.h"
#include "main_window.h"
#include "render/adventure_notes.h"

namespace {
namespace fs = std::filesystem;

struct Record {
    u32 hash;
    const char* title;
};

// The game's Int64 save keys. Names from the public demo of https://onif.info/ocarina/.
constexpr std::array<Record, 23> OcarinaRecords{{
    {0x8bc1257a, "Morts"},
    {0xf7cb0ee2, "Ennemis vaincus"},
    {0x15ea6aa2, "Skulltulas d'or"},
    {0x4766a777, "Coffres"},
    {0x0b7f5726, "Rubis ramassés"},
    {0xcb183657, "Pots cassés"},
    {0x824b47b1, "Herbe coupée"},
    {0x30bbe1d8, "Pierres soulevées"},
    {0x2a24450a, "Coups d'épée"},
    {0x4ac3ff5d, "Roulades"},
    {0xa6454431, "Sauts"},
    {0x3eeafadd, "Distance parcourue"},
    {0x392c7b86, "Temps sous l'eau"},
    {0xe8c057bc, "Vol de cocotte"},
    {0x1957b8c5, "Objets utilisés"},
    {0xf5f1954d, "Magie dépensée"},
    {0xd457ee3a, "Ocarina joué"},
    {0x64b98a4e, "Discussions"},
    {0xc0be46bc, "Discussions avec Navi"},
    {0x70d7014b, "Pierres à potins"},
    {0x3f31ad53, "Mini-jeux"},
    {0xcaa00e60, "Pêche"},
    {0xfd680ada, "Amiibo"},
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
    label = new QLabel(this);
    label->setStyleSheet(QStringLiteral("color: white; font-size: 13px;"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->addWidget(label);
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &AdventureNotes::Refresh);
    timer->start(3000);
    Refresh();
}

void AdventureNotes::Refresh() {
    const auto path = game ? NewestSave(*game) : std::nullopt;
    const auto records = path ? ReadRecords(*game, *path) : std::nullopt;
    if (!records) {
        label->setText(tr("Aucune sauvegarde trouvée pour ce jeu."));
        adjustSize();
        return;
    }
    const auto find = [&records](u32 hash) {
        const auto it = records->find(hash);
        return it != records->end() ? it->second : s64{0};
    };
    const s64 seconds = find(game->play_time_hash);
    QString text = QStringLiteral("<b>%1</b><br>%2<br>")
                       .arg(QString::fromUtf8(game->title))
                       .arg(QString::fromStdString(path->parent_path().filename().string()));
    text += QStringLiteral("Temps de jeu : <b>%1 h %2</b><br>")
                .arg(seconds / 3600)
                .arg((seconds % 3600) / 60, 2, 10, QLatin1Char('0'));
    for (const Record& record : game->records) {
        text += QStringLiteral("%1 : <b>%2</b><br>")
                    .arg(QString::fromUtf8(record.title))
                    .arg(find(record.hash));
    }
    label->setText(text);
    adjustSize();
}

void AdventureNotes::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor{20, 24, 20, 210});
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), 10.0, 10.0);
}

void AdventureNotes::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (QGuiApplication::platformName().startsWith(QStringLiteral("wayland")) && windowHandle() &&
        windowHandle()->startSystemMove()) {
        return;
    }
    drag_start = event->pos();
}

void AdventureNotes::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - drag_start);
    }
}

void AdventureNotes::closeEvent(QCloseEvent*) {
    emit closed();
}
