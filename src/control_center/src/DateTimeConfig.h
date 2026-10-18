#pragma once

#include <QDateTime>
#include <QObject>
#include <QQmlEngine>
#include <QStringList>

// Date & Time page, through systemd-timedated: time zone, network time
// (NTP) and setting the clock. Changing them needs administrator rights,
// which polkit grants or asks for.
class DateTimeConfig : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString timezone READ timezone NOTIFY changed)
    Q_PROPERTY(bool ntp READ ntp NOTIFY changed)
    Q_PROPERTY(bool canNtp READ canNtp NOTIFY changed)
    Q_PROPERTY(bool ntpSynchronized READ ntpSynchronized NOTIFY changed)
    Q_PROPERTY(bool localRtc READ localRtc NOTIFY changed)
    Q_PROPERTY(QStringList timezones READ timezones CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit DateTimeConfig(QObject *parent = nullptr);

    bool available() const { return m_available; }
    QString timezone() const { return m_timezone; }
    bool ntp() const { return m_ntp; }
    bool canNtp() const { return m_canNtp; }
    bool ntpSynchronized() const { return m_synchronized; }
    bool localRtc() const { return m_localRtc; }
    QStringList timezones() const;
    bool busy() const { return m_busy > 0; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setTimezone(const QString &zone);
    Q_INVOKABLE void setNtp(bool enabled);
    Q_INVOKABLE void setDateTime(const QDateTime &local);

signals:
    void changed();
    void busyChanged();
    void errorOccurred(const QString &message);
    void message(const QString &text);

private:
    void timedate(const QString &method, const QVariantList &args, const QString &failure);

    bool m_available = false;
    QString m_timezone;
    bool m_ntp = false;
    bool m_canNtp = false;
    bool m_synchronized = false;
    bool m_localRtc = false;
    int m_busy = 0;
    mutable QStringList m_zones;   // fetched once, on first use
};
