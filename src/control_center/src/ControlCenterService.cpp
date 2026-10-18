#include "ControlCenterService.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>

ControlCenterService::ControlCenterService(QObject *parent)
    : QObject(parent)
{
}

ControlCenterService *ControlCenterService::instance()
{
    static ControlCenterService *self = new ControlCenterService(QCoreApplication::instance());
    return self;
}

ControlCenterService *ControlCenterService::create(QQmlEngine *, QJSEngine *)
{
    ControlCenterService *self = instance();
    QJSEngine::setObjectOwnership(self, QJSEngine::CppOwnership);
    return self;
}

bool ControlCenterService::registerOnBus()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return true;   // no session bus: nothing to coordinate with
    new ControlCenterAdaptor(this);
    if (!bus.registerService(QLatin1String(ServiceName)))
        return false;
    bus.registerObject(QStringLiteral("/"), this);
    return true;
}

bool ControlCenterService::forwardToRunningInstance(const QString &page)
{
    QDBusMessage message = QDBusMessage::createMethodCall(QLatin1String(ServiceName), QStringLiteral("/"),
                                                          QLatin1String(ServiceName), QStringLiteral("ShowPage"));
    message << page;
    return QDBusConnection::sessionBus().call(message, QDBus::Block, 3000).type() == QDBusMessage::ReplyMessage;
}

QString ControlCenterService::notificationServer() const
{
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"), QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("GetServerInformation"));
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 1500);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() < 3)
        return {};
    return reply.arguments().at(0).toString() + QLatin1Char(' ') + reply.arguments().at(2).toString();
}

bool ControlCenterService::sendTestNotification() const
{
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"), QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("Notify"));
    message << QStringLiteral("Control Center") << 0u << QStringLiteral("preferences-system")
            << QStringLiteral("Test notification")
            << QStringLiteral("Notifications appear in the bottom right corner.")
            << QStringList() << QVariantMap() << -1;
    return QDBusConnection::sessionBus().call(message, QDBus::Block, 1500).type() == QDBusMessage::ReplyMessage;
}

ControlCenterAdaptor::ControlCenterAdaptor(ControlCenterService *service)
    : QDBusAbstractAdaptor(service)
    , m_service(service)
{
}

void ControlCenterAdaptor::ShowPage(const QString &page)
{
    emit m_service->showRequested(page);
}
