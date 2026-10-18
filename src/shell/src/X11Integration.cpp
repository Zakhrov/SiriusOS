#include "X11Integration.h"

#include <QGuiApplication>
#include <QHash>
#include <QRectF>
#include <QVariant>
#include <QScreen>

namespace {
X11Integration *s_instance = nullptr;
}

X11Integration::X11Integration(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    if (auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>()) {
        m_connection = x11->connection();
        const xcb_setup_t *setup = xcb_get_setup(m_connection);
        m_root = xcb_setup_roots_iterator(setup).data->root;
    }
}

X11Integration *X11Integration::instance()
{
    if (!s_instance)
        s_instance = new X11Integration(qGuiApp);
    return s_instance;
}

X11Integration *X11Integration::create(QQmlEngine *, QJSEngine *)
{
    X11Integration *self = instance();
    QJSEngine::setObjectOwnership(self, QJSEngine::CppOwnership);
    return self;
}

xcb_atom_t X11Integration::atom(const char *name)
{
    static QHash<QByteArray, xcb_atom_t> cache;
    if (!m_connection)
        return XCB_ATOM_NONE;
    const QByteArray key(name);
    auto it = cache.constFind(key);
    if (it != cache.constEnd())
        return *it;

    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(
        m_connection, xcb_intern_atom(m_connection, 0, key.size(), key.constData()), nullptr);
    xcb_atom_t result = reply ? reply->atom : XCB_ATOM_NONE;
    free(reply);
    cache.insert(key, result);
    return result;
}

void X11Integration::setWindowType(xcb_window_t window, const char *type)
{
    setAtomListProperty(window, "_NET_WM_WINDOW_TYPE", { type });
}

void X11Integration::setAtomListProperty(xcb_window_t window, const char *property,
                                         const QList<const char *> &values)
{
    QList<xcb_atom_t> atoms;
    for (const char *value : values)
        atoms.append(atom(value));
    xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, window, atom(property),
                        XCB_ATOM_ATOM, 32, atoms.size(), atoms.constData());
}

void X11Integration::setupDesktop(QWindow *window)
{
    if (!m_connection || !window)
        return;
    const xcb_window_t id = window->winId();
    setWindowType(id, "_NET_WM_WINDOW_TYPE_DESKTOP");
    setAtomListProperty(id, "_NET_WM_STATE",
                        { "_NET_WM_STATE_BELOW", "_NET_WM_STATE_SKIP_TASKBAR",
                          "_NET_WM_STATE_SKIP_PAGER", "_NET_WM_STATE_STICKY" });
    xcb_flush(m_connection);
}

void X11Integration::setupDock(QWindow *window, Edge edge, int thickness)
{
    if (!m_connection || !window)
        return;
    const xcb_window_t id = window->winId();
    setWindowType(id, "_NET_WM_WINDOW_TYPE_DOCK");
    setAtomListProperty(id, "_NET_WM_STATE",
                        { "_NET_WM_STATE_ABOVE", "_NET_WM_STATE_SKIP_TASKBAR",
                          "_NET_WM_STATE_SKIP_PAGER", "_NET_WM_STATE_STICKY" });

    // _NET_WM_STRUT_PARTIAL: left, right, top, bottom, then start/end pairs.
    const QRect g = window->screen() ? window->screen()->geometry() : window->geometry();
    const qreal dpr = window->devicePixelRatio();
    const auto px = [dpr](int v) { return quint32(qRound(v * dpr)); };
    quint32 strut[12] = {};
    switch (edge) {
    case Left:   strut[0] = px(thickness); strut[4] = px(g.top()); strut[5] = px(g.bottom()); break;
    case Right:  strut[1] = px(thickness); strut[6] = px(g.top()); strut[7] = px(g.bottom()); break;
    case Top:    strut[2] = px(thickness); strut[8] = px(g.left()); strut[9] = px(g.right()); break;
    case Bottom: strut[3] = px(thickness); strut[10] = px(g.left()); strut[11] = px(g.right()); break;
    }
    xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, id, atom("_NET_WM_STRUT_PARTIAL"),
                        XCB_ATOM_CARDINAL, 32, 12, strut);
    xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, id, atom("_NET_WM_STRUT"),
                        XCB_ATOM_CARDINAL, 32, 4, strut);
    xcb_flush(m_connection);
}

void X11Integration::setupPopup(QWindow *window, bool notification)
{
    if (!m_connection || !window)
        return;
    const xcb_window_t id = window->winId();
    setWindowType(id, notification ? "_NET_WM_WINDOW_TYPE_NOTIFICATION"
                                   : "_NET_WM_WINDOW_TYPE_POPUP_MENU");
    setAtomListProperty(id, "_NET_WM_STATE",
                        { "_NET_WM_STATE_ABOVE", "_NET_WM_STATE_SKIP_TASKBAR",
                          "_NET_WM_STATE_SKIP_PAGER" });
    xcb_flush(m_connection);
}

void X11Integration::setBlurBehind(QWindow *window, bool enabled, const QVariantList &rects)
{
    if (!m_connection || !window)
        return;
    const xcb_window_t id = window->winId();
    const xcb_atom_t property = atom("_KDE_NET_WM_BLUR_BEHIND_REGION");
    if (!enabled) {
        xcb_delete_property(m_connection, id, property);
        xcb_flush(m_connection);
        return;
    }
    // Device pixels: the X window's own coordinates.
    const qreal scale = window->devicePixelRatio();
    QList<uint32_t> values;
    for (const QVariant &value : rects) {
        const QRectF rect = value.toRectF();
        if (rect.isEmpty())
            continue;
        values << uint32_t(qRound(rect.x() * scale)) << uint32_t(qRound(rect.y() * scale))
               << uint32_t(qRound(rect.width() * scale)) << uint32_t(qRound(rect.height() * scale));
    }
    // An empty list means the whole window, but only when no rectangles were
    // asked for at all; a list whose rectangles are all empty means nothing.
    if (values.isEmpty() && !rects.isEmpty()) {
        xcb_delete_property(m_connection, id, property);
    } else {
        xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, id, property, XCB_ATOM_CARDINAL, 32,
                            uint32_t(values.size()), values.constData());
    }
    xcb_flush(m_connection);
}
