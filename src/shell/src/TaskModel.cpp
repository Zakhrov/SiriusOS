#include "TaskModel.h"
#include "X11Integration.h"

#include <QCoreApplication>

#include <memory>

namespace {

struct FreeDeleter {
    void operator()(void *p) const { free(p); }
};
template <typename T>
using XcbReply = std::unique_ptr<T, FreeDeleter>;

XcbReply<xcb_get_property_reply_t> getProperty(xcb_window_t window, xcb_atom_t property,
                                               xcb_atom_t type, quint32 length = 1024)
{
    xcb_connection_t *c = X11Integration::instance()->connection();
    return XcbReply<xcb_get_property_reply_t>(xcb_get_property_reply(
        c, xcb_get_property(c, 0, window, property, type, 0, length), nullptr));
}

QList<xcb_atom_t> atomList(xcb_window_t window, const char *property)
{
    X11Integration *x11 = X11Integration::instance();
    QList<xcb_atom_t> result;
    auto reply = getProperty(window, x11->atom(property), XCB_ATOM_ATOM);
    if (reply && reply->format == 32) {
        auto *atoms = static_cast<xcb_atom_t *>(xcb_get_property_value(reply.get()));
        result = QList<xcb_atom_t>(atoms, atoms + xcb_get_property_value_length(reply.get()) / 4);
    }
    return result;
}

} // namespace

TaskModel::TaskModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(0);
    connect(&m_refreshTimer, &QTimer::timeout, this, &TaskModel::refresh);

    X11Integration *x11 = X11Integration::instance();
    if (!x11->isAvailable())
        return;

    // Add PropertyChange to whatever mask Qt already selected on the root.
    xcb_connection_t *c = x11->connection();
    XcbReply<xcb_get_window_attributes_reply_t> attrs(xcb_get_window_attributes_reply(
        c, xcb_get_window_attributes(c, x11->rootWindow()), nullptr));
    const quint32 mask = (attrs ? attrs->your_event_mask : 0) | XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes(c, x11->rootWindow(), XCB_CW_EVENT_MASK, &mask);
    xcb_flush(c);

    QCoreApplication::instance()->installNativeEventFilter(this);
    scheduleRefresh();
}

TaskModel::~TaskModel()
{
    if (QCoreApplication::instance())
        QCoreApplication::instance()->removeNativeEventFilter(this);
}

int TaskModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_tasks.size();
}

QVariant TaskModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_tasks.size())
        return {};
    const Task &task = m_tasks.at(index.row());
    switch (role) {
    case WindowIdRole: return QVariant::fromValue(quint32(task.id));
    case Qt::DisplayRole:
    case TitleRole: return task.title;
    case ActiveRole: return task.id == m_activeWindow;
    case MinimizedRole: return task.minimized;
    case AttentionRole: return task.attention;
    case IconSourceRole:
        if (m_demo)
            return QString();
        return QStringLiteral("image://windowicon/%1/%2").arg(task.id).arg(task.iconSerial);
    }
    return {};
}

QHash<int, QByteArray> TaskModel::roleNames() const
{
    return {
        { WindowIdRole, "windowId" },
        { TitleRole, "title" },
        { ActiveRole, "active" },
        { MinimizedRole, "minimized" },
        { AttentionRole, "attention" },
        { IconSourceRole, "iconSource" },
    };
}

void TaskModel::setDemo(bool demo)
{
    if (m_demo == demo)
        return;
    m_demo = demo;
    emit demoChanged();
    if (m_demo)
        loadDemoTasks();
    else
        scheduleRefresh();
}

void TaskModel::loadDemoTasks()
{
    beginResetModel();
    m_tasks.clear();
    const QStringList titles = { QStringLiteral("Terminal — ~/SiriusOS"),
                                 QStringLiteral("Files — Home"),
                                 QStringLiteral("Noir Settings") };
    for (int i = 0; i < titles.size(); ++i) {
        Task task;
        task.id = xcb_window_t(i + 1);
        task.title = titles.at(i);
        task.minimized = i == 2;
        m_tasks.append(task);
    }
    m_activeWindow = 1;
    endResetModel();
    emit countChanged();
}

void TaskModel::scheduleRefresh()
{
    if (!m_demo)
        m_refreshTimer.start();
}

int TaskModel::rowOf(xcb_window_t id) const
{
    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks.at(i).id == id)
            return i;
    }
    return -1;
}

void TaskModel::readTask(Task &task)
{
    X11Integration *x11 = X11Integration::instance();

    task.title.clear();
    if (auto reply = getProperty(task.id, x11->atom("_NET_WM_NAME"), x11->atom("UTF8_STRING"))) {
        task.title = QString::fromUtf8(static_cast<const char *>(xcb_get_property_value(reply.get())),
                                       xcb_get_property_value_length(reply.get()));
    }
    if (task.title.isEmpty()) {
        if (auto reply = getProperty(task.id, XCB_ATOM_WM_NAME, XCB_ATOM_STRING)) {
            task.title = QString::fromLatin1(static_cast<const char *>(xcb_get_property_value(reply.get())),
                                             xcb_get_property_value_length(reply.get()));
        }
    }

    const QList<xcb_atom_t> state = atomList(task.id, "_NET_WM_STATE");
    task.minimized = state.contains(x11->atom("_NET_WM_STATE_HIDDEN"));
    task.attention = state.contains(x11->atom("_NET_WM_STATE_DEMANDS_ATTENTION"));

    const QList<xcb_atom_t> types = atomList(task.id, "_NET_WM_WINDOW_TYPE");
    task.skip = state.contains(x11->atom("_NET_WM_STATE_SKIP_TASKBAR"))
        || types.contains(x11->atom("_NET_WM_WINDOW_TYPE_DOCK"))
        || types.contains(x11->atom("_NET_WM_WINDOW_TYPE_DESKTOP"));
}

void TaskModel::refresh()
{
    X11Integration *x11 = X11Integration::instance();
    if (m_demo || !x11->isAvailable())
        return;

    QList<xcb_window_t> clients;
    if (auto reply = getProperty(x11->rootWindow(), x11->atom("_NET_CLIENT_LIST"), XCB_ATOM_WINDOW)) {
        auto *ids = static_cast<xcb_window_t *>(xcb_get_property_value(reply.get()));
        clients = QList<xcb_window_t>(ids, ids + xcb_get_property_value_length(reply.get()) / 4);
    }
    if (auto reply = getProperty(x11->rootWindow(), x11->atom("_NET_ACTIVE_WINDOW"), XCB_ATOM_WINDOW, 1)) {
        if (xcb_get_property_value_length(reply.get()) >= 4)
            m_activeWindow = *static_cast<xcb_window_t *>(xcb_get_property_value(reply.get()));
    }

    QList<Task> tasks;
    xcb_connection_t *c = x11->connection();
    const quint32 mask = XCB_EVENT_MASK_PROPERTY_CHANGE;
    for (xcb_window_t id : clients) {
        const int existing = rowOf(id);
        Task task = existing >= 0 ? m_tasks.at(existing) : Task{};
        task.id = id;
        if (existing < 0)
            xcb_change_window_attributes(c, id, XCB_CW_EVENT_MASK, &mask);
        readTask(task);
        if (!task.skip)
            tasks.append(task);
    }
    xcb_flush(c);

    const int oldCount = m_tasks.size();
    beginResetModel();
    m_tasks = tasks;
    endResetModel();
    if (oldCount != m_tasks.size())
        emit countChanged();
}

bool TaskModel::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *)
{
    if (m_demo || eventType != "xcb_generic_event_t")
        return false;
    auto *event = static_cast<xcb_generic_event_t *>(message);
    if ((event->response_type & ~0x80) != XCB_PROPERTY_NOTIFY)
        return false;

    auto *notify = reinterpret_cast<xcb_property_notify_event_t *>(event);
    X11Integration *x11 = X11Integration::instance();
    if (notify->window == x11->rootWindow()) {
        if (notify->atom == x11->atom("_NET_CLIENT_LIST") || notify->atom == x11->atom("_NET_ACTIVE_WINDOW"))
            scheduleRefresh();
        return false;
    }

    const int row = rowOf(notify->window);
    if (row < 0)
        return false;
    Task &task = m_tasks[row];
    if (notify->atom == x11->atom("_NET_WM_ICON")) {
        ++task.iconSerial;
    } else if (notify->atom == x11->atom("_NET_WM_NAME") || notify->atom == XCB_ATOM_WM_NAME
               || notify->atom == x11->atom("_NET_WM_STATE")) {
        readTask(task);
        if (task.skip) {
            scheduleRefresh();
            return false;
        }
    } else {
        return false;
    }
    emit dataChanged(index(row), index(row));
    return false;
}

void TaskModel::sendClientMessage(xcb_window_t window, xcb_atom_t type, const quint32 (&data)[5])
{
    X11Integration *x11 = X11Integration::instance();
    if (!x11->isAvailable())
        return;
    xcb_client_message_event_t event = {};
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = window;
    event.type = type;
    std::copy(std::begin(data), std::end(data), event.data.data32);
    xcb_send_event(x11->connection(), 0, x11->rootWindow(),
                   XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                   reinterpret_cast<const char *>(&event));
    xcb_flush(x11->connection());
}

void TaskModel::activate(int row)
{
    if (row < 0 || row >= m_tasks.size())
        return;
    if (m_demo) {
        m_activeWindow = m_tasks.at(row).id;
        m_tasks[row].minimized = false;
        emit dataChanged(index(0), index(m_tasks.size() - 1));
        return;
    }
    // Source indication 2 = pager/taskbar, so the WM honours the request.
    sendClientMessage(m_tasks.at(row).id, X11Integration::instance()->atom("_NET_ACTIVE_WINDOW"),
                      { 2, XCB_CURRENT_TIME, 0, 0, 0 });
}

void TaskModel::minimize(int row)
{
    if (row < 0 || row >= m_tasks.size())
        return;
    if (m_demo) {
        m_tasks[row].minimized = true;
        if (m_activeWindow == m_tasks.at(row).id)
            m_activeWindow = 0;
        emit dataChanged(index(0), index(m_tasks.size() - 1));
        return;
    }
    constexpr quint32 IconicState = 3;
    sendClientMessage(m_tasks.at(row).id, X11Integration::instance()->atom("WM_CHANGE_STATE"),
                      { IconicState, 0, 0, 0, 0 });
}

void TaskModel::close(int row)
{
    if (row < 0 || row >= m_tasks.size())
        return;
    if (m_demo) {
        beginRemoveRows({}, row, row);
        m_tasks.removeAt(row);
        endRemoveRows();
        emit countChanged();
        return;
    }
    sendClientMessage(m_tasks.at(row).id, X11Integration::instance()->atom("_NET_CLOSE_WINDOW"),
                      { XCB_CURRENT_TIME, 2, 0, 0, 0 });
}

void TaskModel::toggle(int row)
{
    if (row < 0 || row >= m_tasks.size())
        return;
    const Task &task = m_tasks.at(row);
    if (task.id == m_activeWindow && !task.minimized)
        minimize(row);
    else
        activate(row);
}

WindowIconProvider::WindowIconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage WindowIconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    X11Integration *x11 = X11Integration::instance();
    const xcb_window_t window = id.section(QLatin1Char('/'), 0, 0).toUInt();
    QImage best;
    if (x11->isAvailable() && window) {
        // _NET_WM_ICON is a sequence of [width, height, ARGB pixels...] entries.
        auto reply = getProperty(window, x11->atom("_NET_WM_ICON"), XCB_ATOM_CARDINAL, 0x40000);
        if (reply && reply->format == 32) {
            const auto *data = static_cast<const quint32 *>(xcb_get_property_value(reply.get()));
            const int length = xcb_get_property_value_length(reply.get()) / 4;
            const int wanted = requestedSize.isValid() ? qMax(requestedSize.width(), requestedSize.height()) : 32;
            int pos = 0;
            while (pos + 2 <= length) {
                const int w = int(data[pos]), h = int(data[pos + 1]);
                if (w <= 0 || h <= 0 || pos + 2 + qint64(w) * h > length)
                    break;
                const bool better = best.isNull()
                    || (best.width() < wanted && w > best.width())
                    || (w >= wanted && w < best.width());
                if (better) {
                    QImage image(w, h, QImage::Format_ARGB32);
                    for (int y = 0; y < h; ++y) {
                        auto *line = reinterpret_cast<quint32 *>(image.scanLine(y));
                        std::copy(data + pos + 2 + y * w, data + pos + 2 + (y + 1) * w, line);
                    }
                    best = image;
                }
                pos += 2 + w * h;
            }
        }
    }
    if (best.isNull()) {
        best = QImage(1, 1, QImage::Format_ARGB32);
        best.fill(Qt::transparent);
    } else if (requestedSize.isValid() && best.size() != requestedSize) {
        best = best.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if (size)
        *size = best.size();
    return best;
}
