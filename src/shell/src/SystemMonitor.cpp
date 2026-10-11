#include "SystemMonitor.h"

#include <QDir>
#include <QFile>
#include <QSysInfo>

namespace {

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll().trimmed();
}

constexpr int HistoryLength = 48;

} // namespace

SystemMonitor::SystemMonitor(QObject *parent)
    : QObject(parent)
{
    m_cpuHistory.fill(0.0, HistoryLength);
    m_timer.setInterval(1500);
    connect(&m_timer, &QTimer::timeout, this, &SystemMonitor::poll);
    m_timer.start();
    poll();
}

void SystemMonitor::setInterval(int ms)
{
    if (ms == m_timer.interval() || ms <= 0)
        return;
    m_timer.setInterval(ms);
    emit intervalChanged();
}

QString SystemMonitor::hostName() const
{
    return QSysInfo::machineHostName();
}

QString SystemMonitor::userName() const
{
    return qEnvironmentVariable("USER", QStringLiteral("noir"));
}

QString SystemMonitor::kernel() const
{
    return QSysInfo::kernelType() + QLatin1Char(' ') + QSysInfo::kernelVersion();
}

QString SystemMonitor::homePath() const
{
    return QDir::homePath();
}

void SystemMonitor::poll()
{
    readCpu();
    readMemory();
    readBattery();
    readNetwork();
    m_uptime = qint64(readFile(QStringLiteral("/proc/uptime")).split(' ').value(0).toDouble());
    emit updated();
}

void SystemMonitor::readCpu()
{
    // First line of /proc/stat: "cpu user nice system idle iowait irq softirq steal ..."
    const QByteArray stat = readFile(QStringLiteral("/proc/stat"));
    const QList<QByteArray> fields = stat.left(stat.indexOf('\n')).simplified().split(' ');
    if (fields.size() < 5 || fields.first() != "cpu")
        return;
    quint64 total = 0;
    for (int i = 1; i < fields.size() && i <= 8; ++i)
        total += fields.at(i).toULongLong();
    const quint64 idle = fields.at(4).toULongLong() + (fields.size() > 5 ? fields.at(5).toULongLong() : 0);

    if (m_prevTotal && total > m_prevTotal) {
        const double dTotal = double(total - m_prevTotal);
        const double dIdle = double(idle - m_prevIdle);
        m_cpuLoad = qBound(0.0, 1.0 - dIdle / dTotal, 1.0);
    }
    m_prevTotal = total;
    m_prevIdle = idle;
    m_cpuHistory.removeFirst();
    m_cpuHistory.append(m_cpuLoad);
}

void SystemMonitor::readMemory()
{
    const QList<QByteArray> lines = readFile(QStringLiteral("/proc/meminfo")).split('\n');
    for (const QByteArray &line : lines) {
        const QList<QByteArray> parts = line.simplified().split(' ');
        if (parts.size() < 2)
            continue;
        if (parts.first() == "MemTotal:")
            m_memoryTotal = parts.at(1).toLongLong();
        else if (parts.first() == "MemAvailable:")
            m_memoryAvailable = parts.at(1).toLongLong();
    }
}

void SystemMonitor::readBattery()
{
    m_batteryPresent = false;
    const QDir supplies(QStringLiteral("/sys/class/power_supply"));
    for (const QString &name : supplies.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString base = supplies.filePath(name) + QLatin1Char('/');
        if (readFile(base + QStringLiteral("type")) != "Battery")
            continue;
        m_batteryPresent = true;
        m_batteryPercent = readFile(base + QStringLiteral("capacity")).toInt();
        const QByteArray status = readFile(base + QStringLiteral("status"));
        m_charging = status == "Charging" || status == "Full";
        return;
    }
}

void SystemMonitor::readNetwork()
{
    m_networkInterface.clear();
    const QDir net(QStringLiteral("/sys/class/net"));
    for (const QString &name : net.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::System)) {
        if (name == QLatin1String("lo"))
            continue;
        if (readFile(net.filePath(name) + QStringLiteral("/operstate")) == "up") {
            m_networkInterface = name;
            return;
        }
    }
}
