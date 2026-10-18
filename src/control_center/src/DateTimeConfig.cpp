#include "DateTimeConfig.h"

#include "Platform.h"

#include <QDBusConnection>
#include <QFile>
#include <QTimeZone>

namespace {

const QString Service = QStringLiteral("org.freedesktop.timedate1");
const QString Path = QStringLiteral("/org/freedesktop/timedate1");

QDBusConnection bus() { return QDBusConnection::systemBus(); }

} // namespace

DateTimeConfig::DateTimeConfig(QObject *parent)
    : QObject(parent)
{
    refresh();
}

void DateTimeConfig::refresh()
{
    m_available = bus().isConnected() && Platform::hasService(bus(), Service);
    if (m_available) {
        const QVariantMap p = Platform::getAll(bus(), Service, Path, Service);
        m_available = !p.isEmpty();
        m_timezone = p.value(QStringLiteral("Timezone")).toString();
        m_ntp = p.value(QStringLiteral("NTP")).toBool();
        m_canNtp = p.value(QStringLiteral("CanNTP")).toBool();
        m_synchronized = p.value(QStringLiteral("NTPSynchronized")).toBool();
        m_localRtc = p.value(QStringLiteral("LocalRTC")).toBool();
    }
    if (m_timezone.isEmpty())
        m_timezone = QString::fromUtf8(QTimeZone::systemTimeZoneId());
    emit changed();
}

QStringList DateTimeConfig::timezones() const
{
    if (!m_zones.isEmpty())
        return m_zones;
    if (m_available) {
        const QDBusMessage reply = Platform::call(bus(), Service, Path, Service, QStringLiteral("ListTimezones"));
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty())
            return m_zones = reply.arguments().first().toStringList();
    }
    // Older systemd: read the tz database's own list.
    QStringList zones;
    QFile tab(QStringLiteral("/usr/share/zoneinfo/tzdata.zi"));
    if (tab.open(QIODevice::ReadOnly | QIODevice::Text)) {
        for (const QString &line : QString::fromUtf8(tab.readAll()).split(QLatin1Char('\n'))) {
            if (line.startsWith(QLatin1String("Z ")) || line.startsWith(QLatin1String("L ")))
                zones << line.section(QLatin1Char(' '), line.startsWith(QLatin1Char('Z')) ? 1 : 2, line.startsWith(QLatin1Char('Z')) ? 1 : 2);
        }
    }
    if (zones.isEmpty()) {
        for (const QByteArray &id : QTimeZone::availableTimeZoneIds())
            zones << QString::fromUtf8(id);
    }
    zones.removeDuplicates();
    zones.sort();
    return m_zones = zones;
}

void DateTimeConfig::timedate(const QString &method, const QVariantList &args, const QString &failure)
{
    ++m_busy;
    emit busyChanged();
    Platform::callAsync(this, bus(), Service, Path, Service, method, args,
                        [this, failure](const QString &error, const QDBusMessage &) {
                            --m_busy;
                            emit busyChanged();
                            if (!error.isEmpty())
                                emit errorOccurred(failure + QStringLiteral(": ") + error);
                            refresh();
                        });
}

void DateTimeConfig::setTimezone(const QString &zone)
{
    if (zone != m_timezone)
        timedate(QStringLiteral("SetTimezone"), { zone, true }, QStringLiteral("Could not change the time zone"));
}

void DateTimeConfig::setNtp(bool enabled)
{
    timedate(QStringLiteral("SetNTP"), { enabled, true },
             enabled ? QStringLiteral("Could not turn on network time") : QStringLiteral("Could not turn off network time"));
}

void DateTimeConfig::setDateTime(const QDateTime &local)
{
    if (m_ntp) {
        emit errorOccurred(QStringLiteral("Turn off network time to set the clock by hand."));
        return;
    }
    const qint64 usec = local.toMSecsSinceEpoch() * 1000;
    timedate(QStringLiteral("SetTime"), { usec, false, true }, QStringLiteral("Could not set the clock"));
}
