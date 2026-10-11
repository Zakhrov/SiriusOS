#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QWindow>

#include <xcb/xcb.h>

// Thin bridge between QML windows and the X11 server. Lets the shell declare
// its surfaces to NoirWM using EWMH so the window manager stacks and reserves
// space for them correctly (desktop always below, taskbar always on top).
class X11Integration : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool available READ isAvailable CONSTANT)

public:
    enum Edge { Top, Bottom, Left, Right };
    Q_ENUM(Edge)

    // No default argument: QML must go through create() and share instance().
    explicit X11Integration(QObject *parent);

    static X11Integration *instance();
    static X11Integration *create(QQmlEngine *, QJSEngine *);

    bool isAvailable() const { return m_connection != nullptr; }
    xcb_connection_t *connection() const { return m_connection; }
    xcb_window_t rootWindow() const { return m_root; }
    xcb_atom_t atom(const char *name);

    // Mark a window as the desktop surface (_NET_WM_WINDOW_TYPE_DESKTOP).
    Q_INVOKABLE void setupDesktop(QWindow *window);
    // Mark a window as a dock and reserve `thickness` pixels on `edge`.
    Q_INVOKABLE void setupDock(QWindow *window, Edge edge, int thickness);
    // Popup surfaces (menus, notifications): skip taskbar/pager, keep above.
    Q_INVOKABLE void setupPopup(QWindow *window, bool notification = false);

private:
    void setWindowType(xcb_window_t window, const char *type);
    void setAtomListProperty(xcb_window_t window, const char *property, const QList<const char *> &values);

    xcb_connection_t *m_connection = nullptr;
    xcb_window_t m_root = 0;
};
