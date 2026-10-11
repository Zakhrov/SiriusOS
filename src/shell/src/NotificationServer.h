#pragma once

#include <QAbstractListModel>
#include <QDBusAbstractAdaptor>
#include <QHash>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantMap>

// org.freedesktop.Notifications server. Notifications sent by applications
// over D-Bus (or posted by the shell itself) are exposed as a model that the
// bottom-right notification stack renders.
class NotificationServer : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool registered READ isRegistered CONSTANT)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        AppNameRole,
        SummaryRole,
        BodyRole,
        IconRole,
        UrgencyRole,
        ActionsRole,
        ProgressRole,
    };

    enum CloseReason { Expired = 1, Dismissed = 2, Closed = 3 };

    // No default argument: QML must go through create() and share instance().
    explicit NotificationServer(QObject *parent);

    static NotificationServer *instance();
    static NotificationServer *create(QQmlEngine *, QJSEngine *);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool isRegistered() const { return m_registered; }

    uint notify(const QString &appName, uint replacesId, const QString &icon, const QString &summary,
                const QString &body, const QStringList &actions, const QVariantMap &hints, int timeout);
    void closeNotification(uint id, CloseReason reason);

    // QML entry points.
    Q_INVOKABLE uint post(const QString &summary, const QString &body = {},
                          const QString &icon = {}, int timeout = -1);
    Q_INVOKABLE uint send(const QString &appName, const QString &summary, const QString &body,
                          const QString &icon, const QStringList &actions = {},
                          const QVariantMap &hints = {}, int timeout = -1);
    Q_INVOKABLE void dismiss(uint id);
    Q_INVOKABLE void clearAll();
    Q_INVOKABLE void invokeAction(uint id, const QString &key);

signals:
    void countChanged();
    void notificationClosed(uint id, uint reason);
    void actionInvoked(uint id, const QString &actionKey);

private:
    struct Notification {
        uint id = 0;
        QString appName;
        QString summary;
        QString body;
        QString icon;
        int urgency = 1;
        QVariantList actions;
        int progress = -1;
        QTimer *timer = nullptr;
    };

    int rowOf(uint id) const;

    QList<Notification> m_notifications;
    uint m_nextId = 1;
    bool m_registered = false;
};

class NotificationAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")

public:
    explicit NotificationAdaptor(NotificationServer *server);

public slots:
    QStringList GetCapabilities();
    uint Notify(const QString &app_name, uint replaces_id, const QString &app_icon,
                const QString &summary, const QString &body, const QStringList &actions,
                const QVariantMap &hints, int expire_timeout);
    void CloseNotification(uint id);
    QString GetServerInformation(QString &vendor, QString &version, QString &spec_version);

signals:
    void NotificationClosed(uint id, uint reason);
    void ActionInvoked(uint id, const QString &action_key);

private:
    NotificationServer *m_server;
};
