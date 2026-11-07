// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>

#include "common/common_types.h"

class MainWindow;
class QLabel;
class QTimer;

struct GameNotes;

/// A game's adventure records, read from its newest save. Nothing is written: the numbers change
/// when the game saves. A game is added with one entry of the table in adventure_notes.cpp.
class AdventureNotes : public QWidget {
    Q_OBJECT

public:
    AdventureNotes(MainWindow* parent, u64 program_id);

    /// Whether the panel knows this game's save.
    [[nodiscard]] static bool Supports(u64 program_id);

signals:
    void closed();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void Refresh();

    const GameNotes* game = nullptr;
    QLabel* label = nullptr;
    QTimer* timer = nullptr;
    QPoint drag_start;
};
