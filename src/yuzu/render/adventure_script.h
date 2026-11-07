// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

class QJSEngine;
class QJSValue;

/// One line of the panel: a value, a tick (check is 0 or 1) or a bar (done and total).
struct NotesRow {
    QString title;
    QString value;
    int check = -1;
    int done = -1;
    int total = 0;
};

struct NotesView {
    QString name;
    std::vector<NotesRow> rows;
};

struct NotesTab {
    QString name;
    std::vector<NotesView> views;
};

/// What a module's read() returned.
struct NotesReport {
    QString title;
    std::vector<NotesRow> keys; ///< The few numbers the strip keeps in view.
    std::vector<NotesTab> tabs;
};

/// The only things a module can reach: no disk or network beyond this.
class NotesHost : public QObject {
    Q_OBJECT

public:
    NotesHost(QJSEngine* engine, QStringList library_dirs);

    void SetSlot(const QString& directory);

    Q_INVOKABLE QByteArray unzip(const QByteArray& data, int offset, int end);
    Q_INVOKABLE QString utf8(const QByteArray& data, int offset, int size);
    Q_INVOKABLE QByteArray file(const QString& name);
    Q_INVOKABLE void include(const QString& name);

private:
    QJSEngine* engine;
    QStringList libraries;
    QString slot;
};

/// A game's module: a script that knows its save. The emulator knows no save format.
class NotesScript {
public:
    NotesScript();
    ~NotesScript();

    [[nodiscard]] bool Load(const QString& path, const QStringList& library_dirs, QString& error);
    [[nodiscard]] std::optional<NotesReport> Read(const QString& path, QString& error);

    [[nodiscard]] const QString& Title() const {
        return title;
    }
    [[nodiscard]] const QString& Accent() const {
        return accent;
    }
    [[nodiscard]] const QString& Folder() const {
        return folder;
    }
    [[nodiscard]] const QString& File() const {
        return file;
    }

private:
    std::unique_ptr<QJSEngine> engine;
    std::unique_ptr<NotesHost> host;
    QString title;
    QString accent;
    QString folder;
    QString file;
};
