#pragma once

#include <QDBusAbstractAdaptor>
#include <QObject>
#include <QQmlEngine>

// org.siriusos.NoirControlCenter on the session bus: keeps the Control
// Center to one window. Starting it again (from the shell's tray, say) asks
// the running instance to show a page instead of opening a second window.
class ControlCenterService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString initialPage READ initialPage CONSTANT)

public:
    static constexpr const char *ServiceName = "org.siriusos.NoirControlCenter";

    static ControlCenterService *instance();
    static ControlCenterService *create(QQmlEngine *, QJSEngine *);

    // Registers the service. Returns false when another instance holds it.
    bool registerOnBus();
    // Asks the instance that holds the service to show `page`.
    static bool forwardToRunningInstance(const QString &page);

    QString initialPage() const { return m_initialPage; }

    // The notification daemon's name and version, or "" when none is running.
    Q_INVOKABLE QString notificationServer() const;
    Q_INVOKABLE bool sendTestNotification() const;
    void setInitialPage(const QString &page) { m_initialPage = page; }

signals:
    // QML switches to the page and raises the window.
    void showRequested(const QString &page);

private:
    // No default argument: an inherited or default constructor would let QML
    // build a second instance instead of going through create().
    explicit ControlCenterService(QObject *parent);
    QString m_initialPage;
};

class ControlCenterAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.siriusos.NoirControlCenter")

public:
    explicit ControlCenterAdaptor(ControlCenterService *service);

public slots:
    void ShowPage(const QString &page);

private:
    ControlCenterService *m_service;
};
