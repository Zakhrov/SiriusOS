#include "SystemInfo.h"

#include "GraphicsConfig.h"
#include "Platform.h"

#include <QClipboard>
#include <QFile>
#include <QGuiApplication>
#include <QScreen>
#include <QSet>
#include <QStorageInfo>
#include <QSysInfo>
#include <QTimer>

namespace {

QString readFirstLine(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readLine()).trimmed();
}

QString osRelease(const QString &key)
{
    QFile file(QStringLiteral("/etc/os-release"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith(key + QLatin1Char('='))) {
            QString value = line.mid(key.size() + 1);
            if (value.size() >= 2 && value.startsWith(QLatin1Char('"')))
                value = value.mid(1, value.size() - 2);
            return value;
        }
    }
    return {};
}

} // namespace

SystemInfo::SystemInfo(QObject *parent)
    : QObject(parent)
{
    m_osName = osRelease(QStringLiteral("PRETTY_NAME"));
    if (m_osName.isEmpty())
        m_osName = QSysInfo::prettyProductName();
    m_osVersion = osRelease(QStringLiteral("VERSION_ID"));

    // Processor: model name, physical cores (unique physical id + core id
    // pairs) and logical threads.
    QFile cpuinfo(QStringLiteral("/proc/cpuinfo"));
    if (cpuinfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QSet<QString> cores;
        QString physical;
        for (const QString &line : QString::fromUtf8(cpuinfo.readAll()).split(QLatin1Char('\n'))) {
            const int colon = line.indexOf(QLatin1Char(':'));
            if (colon < 0)
                continue;
            const QString key = line.left(colon).trimmed();
            const QString value = line.mid(colon + 1).trimmed();
            if (key == QLatin1String("model name") && m_cpuModel.isEmpty())
                m_cpuModel = value;
            else if (key == QLatin1String("processor"))
                ++m_cpuThreads;
            else if (key == QLatin1String("physical id"))
                physical = value;
            else if (key == QLatin1String("core id"))
                cores.insert(physical + QLatin1Char('/') + value);
        }
        m_cpuCores = cores.isEmpty() ? m_cpuThreads : int(cores.size());
    }

    QFile meminfo(QStringLiteral("/proc/meminfo"));
    if (meminfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString first = QString::fromUtf8(meminfo.readLine());
        m_memory = formatBytes(first.section(QLatin1Char(':'), 1).trimmed().section(QLatin1Char(' '), 0, 0).toLongLong() * 1024);
    }

    const QString vendor = readFirstLine(QStringLiteral("/sys/class/dmi/id/sys_vendor"));
    const QString product = readFirstLine(QStringLiteral("/sys/class/dmi/id/product_name"));
    m_machine = (vendor + QLatin1Char(' ') + product).trimmed();

    QString error;
    m_virtualization = Platform::run(QStringLiteral("systemd-detect-virt"), {}, &error).trimmed();
    if (m_virtualization == QLatin1String("none"))
        m_virtualization.clear();

    if (xcb_connection_t *c = Platform::xConnection()) {
        const xcb_setup_t *setup = xcb_get_setup(c);
        const QString serverVendor = QString::fromLatin1(xcb_setup_vendor(setup), xcb_setup_vendor_length(setup));
        const quint32 r = setup->release_number;
        m_windowSystem = QStringLiteral("X11 · %1 %2.%3.%4").arg(serverVendor).arg(r / 10000000)
                             .arg((r / 100000) % 100).arg((r / 1000) % 100);
    } else {
        m_windowSystem = QStringLiteral("X11");
    }

    m_gpus = GraphicsConfig::enumerateDevices();

    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &SystemInfo::uptimeChanged);
    timer->start(30000);
}

QString SystemInfo::kernel() const
{
    return QSysInfo::kernelType() + QLatin1Char(' ') + QSysInfo::kernelVersion();
}

QString SystemInfo::architecture() const
{
    return QSysInfo::currentCpuArchitecture();
}

QString SystemInfo::hostName() const
{
    return QSysInfo::machineHostName();
}

QString SystemInfo::noirVersion() const
{
    return QStringLiteral(NOIR_VERSION);
}

QString SystemInfo::qtVersion() const
{
    return QString::fromLatin1(qVersion());
}

QVariantList SystemInfo::screens() const
{
    QVariantList list;
    if (!qGuiApp)
        return list;
    for (QScreen *screen : QGuiApplication::screens()) {
        QVariantMap entry;
        entry[QStringLiteral("name")] = screen->name();
        entry[QStringLiteral("model")] = (screen->manufacturer() + QLatin1Char(' ') + screen->model()).trimmed();
        entry[QStringLiteral("resolution")] = QStringLiteral("%1 × %2").arg(screen->size().width()).arg(screen->size().height());
        entry[QStringLiteral("refresh")] = QStringLiteral("%1 Hz").arg(qRound(screen->refreshRate()));
        list << entry;
    }
    return list;
}

QVariantList SystemInfo::disks() const
{
    QVariantList list;
    QSet<QString> seen;
    for (const QStorageInfo &volume : QStorageInfo::mountedVolumes()) {
        const QString device = QString::fromUtf8(volume.device());
        if (!volume.isValid() || !volume.isReady() || !device.startsWith(QLatin1String("/dev/")) || seen.contains(device))
            continue;
        seen.insert(device);
        QVariantMap entry;
        entry[QStringLiteral("mount")] = volume.rootPath();
        entry[QStringLiteral("device")] = device;
        entry[QStringLiteral("fileSystem")] = QString::fromUtf8(volume.fileSystemType());
        entry[QStringLiteral("total")] = formatBytes(volume.bytesTotal());
        entry[QStringLiteral("free")] = formatBytes(volume.bytesAvailable());
        entry[QStringLiteral("used")] = volume.bytesTotal() > 0
            ? 1.0 - double(volume.bytesAvailable()) / double(volume.bytesTotal()) : 0.0;
        list << entry;
    }
    return list;
}

QString SystemInfo::uptime() const
{
    const qint64 seconds = readFirstLine(QStringLiteral("/proc/uptime")).section(QLatin1Char(' '), 0, 0).toDouble();
    const qint64 days = seconds / 86400, hours = (seconds % 86400) / 3600, minutes = (seconds % 3600) / 60;
    if (days > 0)
        return QStringLiteral("%1 d %2 h %3 min").arg(days).arg(hours).arg(minutes);
    return hours > 0 ? QStringLiteral("%1 h %2 min").arg(hours).arg(minutes) : QStringLiteral("%1 min").arg(minutes);
}

QString SystemInfo::summary() const
{
    QStringList lines;
    lines << QStringLiteral("Operating system: %1").arg(m_osName)
          << QStringLiteral("Noir Desktop Environment: %1 (Qt %2)").arg(noirVersion(), qtVersion())
          << QStringLiteral("Kernel: %1 (%2)").arg(kernel(), architecture())
          << QStringLiteral("Window system: %1").arg(m_windowSystem);
    if (!m_machine.isEmpty())
        lines << QStringLiteral("Machine: %1").arg(m_machine);
    if (!m_virtualization.isEmpty())
        lines << QStringLiteral("Virtualization: %1").arg(m_virtualization);
    lines << QStringLiteral("Processor: %1 (%2 cores, %3 threads)").arg(m_cpuModel).arg(m_cpuCores).arg(m_cpuThreads)
          << QStringLiteral("Memory: %1").arg(m_memory);
    for (const QVariant &gpu : m_gpus) {
        const QVariantMap g = gpu.toMap();
        lines << QStringLiteral("Graphics: %1 (%2, %3, Vulkan %4)%5")
                     .arg(g.value(QStringLiteral("name")).toString(), g.value(QStringLiteral("typeName")).toString(),
                          g.value(QStringLiteral("driverName")).toString(), g.value(QStringLiteral("apiVersion")).toString(),
                          g.value(QStringLiteral("active")).toBool() ? QStringLiteral(" [Noir]") : QString());
    }
    for (const QVariant &screen : screens()) {
        const QVariantMap s = screen.toMap();
        lines << QStringLiteral("Display: %1 %2 @ %3").arg(s.value(QStringLiteral("name")).toString(),
                                                           s.value(QStringLiteral("resolution")).toString(),
                                                           s.value(QStringLiteral("refresh")).toString());
    }
    return lines.join(QLatin1Char('\n'));
}

void SystemInfo::copySummary() const
{
    if (qGuiApp)
        QGuiApplication::clipboard()->setText(summary());
}

QString SystemInfo::formatBytes(qint64 bytes)
{
    const char *units[] = { "B", "KiB", "MiB", "GiB", "TiB" };
    double value = double(bytes);
    int unit = 0;
    while (value >= 1024 && unit < 4) {
        value /= 1024;
        ++unit;
    }
    return QStringLiteral("%1 %2").arg(value, 0, 'f', unit >= 3 ? 1 : 0).arg(QLatin1String(units[unit]));
}
