#pragma once

#include <QObject>
#include <QString>

class QQmlApplicationEngine;

// Inspection hooks for driving the shell from outside (the Xvfb harness in
// tools/xvfb, or by hand with dbus-send / qdbus). Only exported when the
// shell is started with --debug-interface:
//
//   service   org.siriusos.NoirShell
//   object    /Debug
//   interface org.siriusos.NoirShell.Debug
class DebugInterface : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.siriusos.NoirShell.Debug")

public:
    explicit DebugInterface(QQmlApplicationEngine *engine, QObject *parent = nullptr);

    bool registerOnBus();

public slots:
    // One line per shell surface: X11 id, screen geometry, state and title.
    QString Windows();
    // Indented Qt Quick item tree with screen coordinates. `window` is a title
    // substring or an X11 window id; empty dumps every surface.
    QString ItemTree(const QString &window, bool visibleOnly);
    // Render a surface to an image file, alpha included (so translucent
    // popups can be inspected without a compositor). Returns an error or "".
    QString Grab(const QString &window, const QString &path);
    // Evaluate a QML/JavaScript expression in the scope of Main.qml, e.g.
    // "shell.startMenuOpen = true" or "Theme.reducedMotion".
    QString Eval(const QString &expression);

private:
    QQmlApplicationEngine *m_engine;
};
