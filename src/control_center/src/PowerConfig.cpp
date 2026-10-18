#include "PowerConfig.h"

#include "NoirConfig.h"
#include "Platform.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDBusObjectPath>

#include <xcb/dpms.h>

#include <cmath>

namespace {

QDBusConnection bus() { return QDBusConnection::systemBus(); }

const QString UPower = QStringLiteral("org.freedesktop.UPower");
const QString Login = QStringLiteral("org.freedesktop.login1");

QString batteryState(uint state)
{
    switch (state) {
    case 1: return QStringLiteral("Charging");
    case 2: return QStringLiteral("Discharging");
    case 3: return QStringLiteral("Empty");
    case 4: return QStringLiteral("Fully charged");
    case 5: return QStringLiteral("Not charging");
    case 6: return QStringLiteral("Discharging slowly");
    default: return QStringLiteral("Unknown");
    }
}

QString duration(qint64 seconds)
{
    if (seconds <= 0)
        return {};
    const qint64 h = seconds / 3600, m = (seconds % 3600) / 60;
    return h > 0 ? QStringLiteral("%1 h %2 min").arg(h).arg(m) : QStringLiteral("%1 min").arg(m);
}

QString technology(uint t)
{
    switch (t) {
    case 1: return QStringLiteral("Lithium-ion");
    case 2: return QStringLiteral("Lithium polymer");
    case 3: return QStringLiteral("Lithium iron phosphate");
    case 4: return QStringLiteral("Lead acid");
    case 5: return QStringLiteral("Nickel cadmium");
    case 6: return QStringLiteral("Nickel metal hydride");
    default: return {};
    }
}

} // namespace

PowerConfig::PowerConfig(QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<QList<QVariantMap>>();
    refresh();
}

void PowerConfig::refresh()
{
    // --- Batteries ---------------------------------------------------------
    m_batteries.clear();
    m_upower = bus().isConnected() && Platform::hasService(bus(), UPower);
    if (m_upower) {
        m_onBattery = Platform::property(bus(), UPower, QStringLiteral("/org/freedesktop/UPower"), UPower,
                                         QStringLiteral("OnBattery")).toBool();
        const QDBusMessage reply = Platform::call(bus(), UPower, QStringLiteral("/org/freedesktop/UPower"), UPower,
                                                  QStringLiteral("EnumerateDevices"));
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
            for (const QDBusObjectPath &path : qdbus_cast<QList<QDBusObjectPath>>(reply.arguments().first())) {
                const QVariantMap d = Platform::getAll(bus(), UPower, path.path(), QStringLiteral("org.freedesktop.UPower.Device"));
                // Type 2 is a battery; only the ones powering the machine.
                if (d.value(QStringLiteral("Type")).toUInt() != 2 || !d.value(QStringLiteral("PowerSupply")).toBool())
                    continue;
                const double full = d.value(QStringLiteral("EnergyFull")).toDouble();
                const double design = d.value(QStringLiteral("EnergyFullDesign")).toDouble();
                QString name = (d.value(QStringLiteral("Vendor")).toString() + QLatin1Char(' ') + d.value(QStringLiteral("Model")).toString()).trimmed();
                if (name.isEmpty())
                    name = d.value(QStringLiteral("NativePath")).toString();
                m_batteries << QVariantMap {
                    { QStringLiteral("name"), name },
                    { QStringLiteral("percentage"), d.value(QStringLiteral("Percentage")).toDouble() / 100.0 },
                    { QStringLiteral("state"), batteryState(d.value(QStringLiteral("State")).toUInt()) },
                    { QStringLiteral("timeToEmpty"), duration(d.value(QStringLiteral("TimeToEmpty")).toLongLong()) },
                    { QStringLiteral("timeToFull"), duration(d.value(QStringLiteral("TimeToFull")).toLongLong()) },
                    { QStringLiteral("rate"), d.value(QStringLiteral("EnergyRate")).toDouble() > 0
                          ? QStringLiteral("%1 W").arg(d.value(QStringLiteral("EnergyRate")).toDouble(), 0, 'f', 1) : QString() },
                    { QStringLiteral("health"), design > 0 ? full / design : -1.0 },
                    { QStringLiteral("capacity"), full > 0 ? QStringLiteral("%1 Wh of %2 Wh design").arg(full, 0, 'f', 1).arg(design, 0, 'f', 1) : QString() },
                    { QStringLiteral("technology"), technology(d.value(QStringLiteral("Technology")).toUInt()) },
                };
            }
        }
    }

    // --- Power profiles ----------------------------------------------------
    // power-profiles-daemon moved to UPower's namespace; support both names.
    m_profileService.clear();
    m_profiles.clear();
    const struct { const char *service, *path, *iface; } candidates[] = {
        { "org.freedesktop.UPower.PowerProfiles", "/org/freedesktop/UPower/PowerProfiles", "org.freedesktop.UPower.PowerProfiles" },
        { "net.hadess.PowerProfiles", "/net/hadess/PowerProfiles", "net.hadess.PowerProfiles" },
    };
    for (const auto &c : candidates) {
        if (!bus().isConnected() || !Platform::hasService(bus(), QLatin1String(c.service)))
            continue;
        const QVariantMap props = Platform::getAll(bus(), QLatin1String(c.service), QLatin1String(c.path), QLatin1String(c.iface));
        if (props.isEmpty())
            continue;
        m_profileService = QLatin1String(c.service);
        m_profilePath = QLatin1String(c.path);
        m_profileIface = QLatin1String(c.iface);
        m_activeProfile = props.value(QStringLiteral("ActiveProfile")).toString();
        const QList<QVariantMap> list = qdbus_cast<QList<QVariantMap>>(props.value(QStringLiteral("Profiles")).value<QDBusArgument>());
        for (const QVariantMap &p : list)
            m_profiles << p.value(QStringLiteral("Profile")).toString();
        break;
    }

    // --- Screen ------------------------------------------------------------
    m_blankMinutes = NoirConfig::instance()->value(QStringLiteral("Power/BlankMinutes"), 10).toInt();
    m_screenOffMinutes = NoirConfig::instance()->value(QStringLiteral("Power/ScreenOffMinutes"), 15).toInt();
    m_dpms = false;
    xcb_connection_t *c = Platform::xConnection();
    if (c && Platform::hasExtension(&xcb_dpms_id)) {
        if (auto *capable = xcb_dpms_capable_reply(c, xcb_dpms_capable(c), nullptr)) {
            m_dpms = capable->capable;
            free(capable);
        }
    }

    // --- Session actions ---------------------------------------------------
    m_actions.clear();
    for (const char *action : { "Suspend", "Hibernate", "Reboot", "PowerOff" }) {
        const QDBusMessage reply = Platform::call(bus(), Login, QStringLiteral("/org/freedesktop/login1"),
                                                  QStringLiteral("org.freedesktop.login1.Manager"),
                                                  QStringLiteral("Can") + QLatin1String(action));
        const QString answer = reply.arguments().value(0).toString();
        m_actions.insert(QStringLiteral("can") + QLatin1String(action), answer == QLatin1String("yes") || answer == QLatin1String("challenge"));
    }
    emit changed();
}

void PowerConfig::setActiveProfile(const QString &profile)
{
    if (m_profileService.isEmpty())
        return;
    Platform::setPropertyAsync(this, bus(), m_profileService, m_profilePath, m_profileIface, QStringLiteral("ActiveProfile"), profile,
                               [this](const QString &error, const QDBusMessage &) {
                                   if (!error.isEmpty())
                                       emit errorOccurred(QStringLiteral("Could not change the power profile: ") + error);
                                   refresh();
                               });
}

void PowerConfig::setBlankMinutes(int minutes)
{
    NoirConfig::instance()->setValue(QStringLiteral("Power/BlankMinutes"), minutes);
    applySaved();
    refresh();
}

void PowerConfig::setScreenOffMinutes(int minutes)
{
    NoirConfig::instance()->setValue(QStringLiteral("Power/ScreenOffMinutes"), minutes);
    applySaved();
    refresh();
}

void PowerConfig::applySaved()
{
    applyScreen(NoirConfig::instance()->value(QStringLiteral("Power/BlankMinutes"), 10).toInt(),
                NoirConfig::instance()->value(QStringLiteral("Power/ScreenOffMinutes"), 15).toInt());
}

void PowerConfig::applyScreen(int blankMinutes, int screenOffMinutes)
{
    xcb_connection_t *c = Platform::xConnection();
    if (!c)
        return;
    // Blanking: the core screen saver, which X shows as a black screen.
    xcb_set_screen_saver(c, int16_t(blankMinutes * 60), 600, XCB_BLANKING_PREFERRED, XCB_EXPOSURES_ALLOWED);
    // Power saving: DPMS standby/suspend/off all at the same moment.
    const uint16_t off = uint16_t(std::min(screenOffMinutes * 60, 65535));
    auto *capable = Platform::hasExtension(&xcb_dpms_id) ? xcb_dpms_capable_reply(c, xcb_dpms_capable(c), nullptr) : nullptr;
    if (capable) {
        if (capable->capable) {
            if (screenOffMinutes > 0) {
                xcb_dpms_set_timeouts(c, off, off, off);
                xcb_dpms_enable(c);
            } else {
                xcb_dpms_disable(c);
            }
        }
        free(capable);
    }
    xcb_flush(c);
}

void PowerConfig::sessionAction(const QString &action)
{
    Platform::callAsync(this, bus(), Login, QStringLiteral("/org/freedesktop/login1"),
                        QStringLiteral("org.freedesktop.login1.Manager"), action, { true },
                        [this, action](const QString &error, const QDBusMessage &) {
                            if (!error.isEmpty())
                                emit errorOccurred(QStringLiteral("%1 failed: %2").arg(action, error));
                        });
}
