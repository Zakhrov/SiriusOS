#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QTimer>

// Lightweight /proc and /sys poller for the desktop widgets and system tray.
class SystemMonitor : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(double cpuLoad READ cpuLoad NOTIFY updated)
    Q_PROPERTY(double memoryLoad READ memoryLoad NOTIFY updated)
    Q_PROPERTY(qint64 memoryUsedMiB READ memoryUsedMiB NOTIFY updated)
    Q_PROPERTY(qint64 memoryTotalMiB READ memoryTotalMiB NOTIFY updated)
    Q_PROPERTY(qint64 uptimeSeconds READ uptimeSeconds NOTIFY updated)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY updated)
    Q_PROPERTY(bool batteryPresent READ batteryPresent NOTIFY updated)
    Q_PROPERTY(bool charging READ charging NOTIFY updated)
    Q_PROPERTY(bool networkOnline READ networkOnline NOTIFY updated)
    Q_PROPERTY(QString networkInterface READ networkInterface NOTIFY updated)
    Q_PROPERTY(QString hostName READ hostName CONSTANT)
    Q_PROPERTY(QString userName READ userName CONSTANT)
    Q_PROPERTY(QString kernel READ kernel CONSTANT)
    Q_PROPERTY(QString homePath READ homePath CONSTANT)
    Q_PROPERTY(QList<double> cpuHistory READ cpuHistory NOTIFY updated)
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged)

public:
    explicit SystemMonitor(QObject *parent = nullptr);

    double cpuLoad() const { return m_cpuLoad; }
    double memoryLoad() const { return m_memoryTotal ? double(m_memoryTotal - m_memoryAvailable) / m_memoryTotal : 0.0; }
    qint64 memoryUsedMiB() const { return (m_memoryTotal - m_memoryAvailable) / 1024; }
    qint64 memoryTotalMiB() const { return m_memoryTotal / 1024; }
    qint64 uptimeSeconds() const { return m_uptime; }
    int batteryPercent() const { return m_batteryPercent; }
    bool batteryPresent() const { return m_batteryPresent; }
    bool charging() const { return m_charging; }
    bool networkOnline() const { return !m_networkInterface.isEmpty(); }
    QString networkInterface() const { return m_networkInterface; }
    QString hostName() const;
    QString userName() const;
    QString kernel() const;
    QString homePath() const;
    QList<double> cpuHistory() const { return m_cpuHistory; }
    int interval() const { return m_timer.interval(); }
    void setInterval(int ms);

signals:
    void updated();
    void intervalChanged();

private:
    void poll();
    void readCpu();
    void readMemory();
    void readBattery();
    void readNetwork();

    QTimer m_timer;
    quint64 m_prevIdle = 0;
    quint64 m_prevTotal = 0;
    double m_cpuLoad = 0.0;
    QList<double> m_cpuHistory;
    qint64 m_memoryTotal = 0;      // KiB
    qint64 m_memoryAvailable = 0;  // KiB
    qint64 m_uptime = 0;
    int m_batteryPercent = -1;
    bool m_batteryPresent = false;
    bool m_charging = false;
    QString m_networkInterface;
};
