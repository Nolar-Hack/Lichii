// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>

#include "common/common_types.h"

class MainWindow;
class QGridLayout;
class QLabel;
class QComboBox;
class QTimer;
class QToolButton;
class QWidget;

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
    bool eventFilter(QObject* object, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void Refresh();
    void StartDrag(QPoint position);

    const GameNotes* game = nullptr;
    QLabel* header = nullptr;
    QComboBox* views = nullptr;
    QWidget* body = nullptr;
    QGridLayout* rows = nullptr;
    QToolButton* collapse = nullptr;
    QTimer* timer = nullptr;
    QPoint drag_start;
    bool dragging = false;
};
