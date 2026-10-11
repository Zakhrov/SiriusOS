#include "NotificationServer.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <QDebug>

namespace {
NotificationServer *s_instance = nullptr;
constexpr int DefaultTimeoutMs = 6000;
constexpr int MaxVisible = 5;
}

NotificationServer::NotificationServer(QObject *parent)
    : QAbstractListModel(parent)
{
    s_instance = this;
    new NotificationAdaptor(this);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected() && bus.registerService(QStringLiteral("org.freedesktop.Notifications"))) {
        m_registered = bus.registerObject(QStringLiteral("/org/freedesktop/Notifications"), this);
    }
    if (!m_registered)
        qWarning() << "NoirShell: notification service not registered:" << bus.lastError().message();
}

NotificationServer *NotificationServer::instance()
{
    if (!s_instance)
        s_instance = new NotificationServer(QCoreApplication::instance());
    return s_instance;
}

NotificationServer *NotificationServer::create(QQmlEngine *, QJSEngine *)
{
    NotificationServer *self = instance();
    QJSEngine::setObjectOwnership(self, QJSEngine::CppOwnership);
    return self;
}

int NotificationServer::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_notifications.size();
}

QVariant NotificationServer::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_notifications.size())
        return {};
    const Notification &n = m_notifications.at(index.row());
    switch (role) {
    case IdRole: return n.id;
    case AppNameRole: return n.appName;
    case Qt::DisplayRole:
    case SummaryRole: return n.summary;
    case BodyRole: return n.body;
    case IconRole: return n.icon;
    case UrgencyRole: return n.urgency;
    case ActionsRole: return n.actions;
    case ProgressRole: return n.progress;
    }
    return {};
}

QHash<int, QByteArray> NotificationServer::roleNames() const
{
    return {
        { IdRole, "notificationId" },
        { AppNameRole, "appName" },
        { SummaryRole, "summary" },
        { BodyRole, "body" },
        { IconRole, "iconName" },
        { UrgencyRole, "urgency" },
        { ActionsRole, "actions" },
        { ProgressRole, "progress" },
    };
}

int NotificationServer::rowOf(uint id) const
{
    for (int i = 0; i < m_notifications.size(); ++i) {
        if (m_notifications.at(i).id == id)
            return i;
    }
    return -1;
}

uint NotificationServer::notify(const QString &appName, uint replacesId, const QString &icon,
                                const QString &summary, const QString &body, const QStringList &actions,
                                const QVariantMap &hints, int timeout)
{
    Notification n;
    n.appName = appName;
    n.summary = summary;
    n.body = body;
    n.icon = icon;
    n.urgency = hints.value(QStringLiteral("urgency"), 1).toInt();
    n.progress = hints.contains(QStringLiteral("value")) ? qBound(0, hints.value(QStringLiteral("value")).toInt(), 100) : -1;
    if (n.icon.isEmpty())
        n.icon = hints.value(QStringLiteral("image-path")).toString();
    // Actions arrive as a flat [key, label, key, label, ...] list.
    for (int i = 0; i + 1 < actions.size(); i += 2)
        n.actions.append(QVariantMap{ { QStringLiteral("key"), actions.at(i) }, { QStringLiteral("label"), actions.at(i + 1) } });

    int row = replacesId ? rowOf(replacesId) : -1;
    if (row >= 0) {
        n.id = replacesId;
        n.timer = m_notifications.at(row).timer;
        m_notifications[row] = n;
        emit dataChanged(index(row), index(row));
    } else {
        n.id = m_nextId++;
        if (m_nextId == 0)
            m_nextId = 1;
        n.timer = new QTimer(this);
        n.timer->setSingleShot(true);
        const uint id = n.id;
        connect(n.timer, &QTimer::timeout, this, [this, id] { closeNotification(id, Expired); });
        beginInsertRows({}, m_notifications.size(), m_notifications.size());
        m_notifications.append(n);
        endInsertRows();
        emit countChanged();
        if (m_notifications.size() > MaxVisible)
            closeNotification(m_notifications.first().id, Expired);
    }

    // Critical notifications (urgency 2) and timeout 0 stay until dismissed.
    if (timeout < 0)
        timeout = DefaultTimeoutMs;
    if (timeout > 0 && n.urgency < 2)
        n.timer->start(timeout);
    else
        n.timer->stop();
    return n.id;
}

void NotificationServer::closeNotification(uint id, CloseReason reason)
{
    const int row = rowOf(id);
    if (row < 0)
        return;
    beginRemoveRows({}, row, row);
    Notification n = m_notifications.takeAt(row);
    endRemoveRows();
    n.timer->deleteLater();
    emit countChanged();
    emit notificationClosed(id, reason);
}

uint NotificationServer::post(const QString &summary, const QString &body, const QString &icon, int timeout)
{
    return notify(QStringLiteral("Noir"), 0, icon, summary, body, {}, {}, timeout);
}

uint NotificationServer::send(const QString &appName, const QString &summary, const QString &body,
                              const QString &icon, const QStringList &actions, const QVariantMap &hints,
                              int timeout)
{
    return notify(appName, 0, icon, summary, body, actions, hints, timeout);
}

void NotificationServer::dismiss(uint id)
{
    closeNotification(id, Dismissed);
}

void NotificationServer::clearAll()
{
    while (!m_notifications.isEmpty())
        closeNotification(m_notifications.last().id, Dismissed);
}

void NotificationServer::invokeAction(uint id, const QString &key)
{
    emit actionInvoked(id, key);
    closeNotification(id, Dismissed);
}

NotificationAdaptor::NotificationAdaptor(NotificationServer *server)
    : QDBusAbstractAdaptor(server)
    , m_server(server)
{
    connect(server, &NotificationServer::notificationClosed, this, &NotificationAdaptor::NotificationClosed);
    connect(server, &NotificationServer::actionInvoked, this, &NotificationAdaptor::ActionInvoked);
}

QStringList NotificationAdaptor::GetCapabilities()
{
    return { QStringLiteral("body"), QStringLiteral("actions"), QStringLiteral("icon-static"),
             QStringLiteral("persistence") };
}

uint NotificationAdaptor::Notify(const QString &app_name, uint replaces_id, const QString &app_icon,
                                 const QString &summary, const QString &body, const QStringList &actions,
                                 const QVariantMap &hints, int expire_timeout)
{
    return m_server->notify(app_name, replaces_id, app_icon, summary, body, actions, hints, expire_timeout);
}

void NotificationAdaptor::CloseNotification(uint id)
{
    m_server->closeNotification(id, NotificationServer::Closed);
}

QString NotificationAdaptor::GetServerInformation(QString &vendor, QString &version, QString &spec_version)
{
    vendor = QStringLiteral("SiriusOS");
    version = QStringLiteral("0.1");
    spec_version = QStringLiteral("1.2");
    return QStringLiteral("Noir Shell");
}
