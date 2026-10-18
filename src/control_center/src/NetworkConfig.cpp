#include "NetworkConfig.h"

#include "Platform.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QNetworkInterface>
#include <QTimer>

#include <algorithm>

using NMSettings = QMap<QString, QVariantMap>;
Q_DECLARE_METATYPE(NMSettings)

namespace {

const QString Service = QStringLiteral("org.freedesktop.NetworkManager");
const QString ManagerPath = QStringLiteral("/org/freedesktop/NetworkManager");
const QString ManagerIface = QStringLiteral("org.freedesktop.NetworkManager");
const QString DeviceIface = QStringLiteral("org.freedesktop.NetworkManager.Device");
const QString WirelessIface = QStringLiteral("org.freedesktop.NetworkManager.Device.Wireless");
const QString SettingsPath = QStringLiteral("/org/freedesktop/NetworkManager/Settings");
const QString ConnectionIface = QStringLiteral("org.freedesktop.NetworkManager.Settings.Connection");

QDBusConnection bus() { return QDBusConnection::systemBus(); }

QString deviceTypeName(uint type)
{
    switch (type) {
    case 1: return QStringLiteral("Ethernet");
    case 2: return QStringLiteral("Wi-Fi");
    case 5: return QStringLiteral("Bluetooth");
    case 8: return QStringLiteral("Mobile broadband");
    case 13: return QStringLiteral("Bridge");
    case 16: return QStringLiteral("Tunnel");
    case 29: return QStringLiteral("WireGuard");
    case 32: return QStringLiteral("Loopback");
    default: return QStringLiteral("Network");
    }
}

QString deviceStateName(uint state)
{
    if (state == 100) return QStringLiteral("Connected");
    if (state >= 40 && state < 100) return QStringLiteral("Connecting…");
    if (state == 110) return QStringLiteral("Disconnecting…");
    if (state == 120) return QStringLiteral("Failed");
    if (state == 30) return QStringLiteral("Disconnected");
    if (state == 20) return QStringLiteral("Unavailable");
    if (state == 10) return QStringLiteral("Not managed");
    return QStringLiteral("Unknown");
}

QList<QDBusObjectPath> objectPaths(const QVariant &v)
{
    return qdbus_cast<QList<QDBusObjectPath>>(v);
}

QStringList addresses(const QString &configPath, const QString &iface)
{
    QStringList out;
    if (configPath.isEmpty() || configPath == QLatin1String("/"))
        return out;
    const QVariantMap config = Platform::getAll(bus(), Service, configPath, iface);
    const QDBusArgument data = config.value(QStringLiteral("AddressData")).value<QDBusArgument>();
    const QList<QVariantMap> list = qdbus_cast<QList<QVariantMap>>(data);
    for (const QVariantMap &a : list)
        out << a.value(QStringLiteral("address")).toString() + QLatin1Char('/') + a.value(QStringLiteral("prefix")).toString();
    return out;
}

NMSettings connectionSettings(const QString &path)
{
    const QDBusMessage reply = Platform::call(bus(), Service, path, ConnectionIface, QStringLiteral("GetSettings"));
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return {};
    return qdbus_cast<NMSettings>(reply.arguments().first());
}

} // namespace

NetworkConfig::NetworkConfig(QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<NMSettings>();
    qDBusRegisterMetaType<QList<QVariantMap>>();
    refresh();
}

void NetworkConfig::refresh()
{
    m_devices.clear();
    m_accessPoints.clear();
    m_connections.clear();
    m_wifiDevice.clear();
    m_available = bus().isConnected() && bus().interface()->isServiceRegistered(Service);
    if (!m_available) {
        m_status = QStringLiteral("NetworkManager is not running, so connections cannot be changed here. The interfaces are shown as the kernel reports them.");
        listInterfacesOnly();
        emit changed();
        return;
    }
    m_status.clear();

    const QVariantMap manager = Platform::getAll(bus(), Service, ManagerPath, ManagerIface);
    m_networking = manager.value(QStringLiteral("NetworkingEnabled")).toBool();
    m_wireless = manager.value(QStringLiteral("WirelessEnabled")).toBool();
    m_wirelessHardware = manager.value(QStringLiteral("WirelessHardwareEnabled")).toBool();

    // Saved connections, by uuid; Wi-Fi ones also by SSID.
    QHash<QString, QString> savedBySsid;
    const QDBusMessage list = Platform::call(bus(), Service, SettingsPath,
                                             QStringLiteral("org.freedesktop.NetworkManager.Settings"), QStringLiteral("ListConnections"));
    if (list.type() == QDBusMessage::ReplyMessage && !list.arguments().isEmpty()) {
        for (const QDBusObjectPath &path : objectPaths(list.arguments().first())) {
            const NMSettings s = connectionSettings(path.path());
            const QVariantMap c = s.value(QStringLiteral("connection"));
            const QString type = c.value(QStringLiteral("type")).toString();
            if (type == QLatin1String("loopback"))
                continue;
            QVariantMap entry {
                { QStringLiteral("id"), c.value(QStringLiteral("id")) },
                { QStringLiteral("uuid"), c.value(QStringLiteral("uuid")) },
                { QStringLiteral("type"), type },
                { QStringLiteral("path"), path.path() },
            };
            if (type == QLatin1String("802-11-wireless")) {
                const QString ssid = QString::fromUtf8(s.value(QStringLiteral("802-11-wireless")).value(QStringLiteral("ssid")).toByteArray());
                entry[QStringLiteral("ssid")] = ssid;
                savedBySsid.insert(ssid, c.value(QStringLiteral("uuid")).toString());
            }
            m_connections << entry;
        }
    }

    QString activeSsid;
    for (const QDBusObjectPath &path : objectPaths(manager.value(QStringLiteral("Devices")))) {
        const QVariantMap d = Platform::getAll(bus(), Service, path.path(), DeviceIface);
        const uint type = d.value(QStringLiteral("DeviceType")).toUInt();
        if (type == 32 || type == 14)   // loopback, generic
            continue;
        const uint state = d.value(QStringLiteral("State")).toUInt();
        const QString active = d.value(QStringLiteral("ActiveConnection")).value<QDBusObjectPath>().path();
        QString connectionName;
        if (!active.isEmpty() && active != QLatin1String("/"))
            connectionName = Platform::property(bus(), Service, active, QStringLiteral("org.freedesktop.NetworkManager.Connection.Active"),
                                                QStringLiteral("Id")).toString();
        QVariantMap entry {
            { QStringLiteral("path"), path.path() },
            { QStringLiteral("interface"), d.value(QStringLiteral("Interface")) },
            { QStringLiteral("type"), deviceTypeName(type) },
            { QStringLiteral("state"), deviceStateName(state) },
            { QStringLiteral("connected"), state == 100 },
            { QStringLiteral("connection"), connectionName },
            { QStringLiteral("mac"), d.value(QStringLiteral("HwAddress")) },
            { QStringLiteral("ipv4"), addresses(d.value(QStringLiteral("Ip4Config")).value<QDBusObjectPath>().path(),
                                                QStringLiteral("org.freedesktop.NetworkManager.IP4Config")) },
            { QStringLiteral("ipv6"), addresses(d.value(QStringLiteral("Ip6Config")).value<QDBusObjectPath>().path(),
                                                QStringLiteral("org.freedesktop.NetworkManager.IP6Config")) },
            { QStringLiteral("managed"), d.value(QStringLiteral("Managed")) },
        };
        if (type == 1) {
            const uint speed = Platform::property(bus(), Service, path.path(), QStringLiteral("org.freedesktop.NetworkManager.Device.Wired"),
                                                  QStringLiteral("Speed")).toUInt();
            if (speed)
                entry[QStringLiteral("speed")] = speed >= 1000 ? QStringLiteral("%1 Gbit/s").arg(speed / 1000.0)
                                                               : QStringLiteral("%1 Mbit/s").arg(speed);
        }
        if (type == 2 && m_wifiDevice.isEmpty()) {
            m_wifiDevice = path.path();
            const QVariantMap w = Platform::getAll(bus(), Service, path.path(), WirelessIface);
            const QString activeAp = w.value(QStringLiteral("ActiveAccessPoint")).value<QDBusObjectPath>().path();
            QHash<QString, QVariantMap> bySsid;   // strongest access point per network
            for (const QDBusObjectPath &ap : objectPaths(w.value(QStringLiteral("AccessPoints")))) {
                const QVariantMap a = Platform::getAll(bus(), Service, ap.path(), QStringLiteral("org.freedesktop.NetworkManager.AccessPoint"));
                const QString ssid = QString::fromUtf8(a.value(QStringLiteral("Ssid")).toByteArray());
                if (ssid.isEmpty())
                    continue;
                const int strength = a.value(QStringLiteral("Strength")).toInt();
                const uint wpa = a.value(QStringLiteral("WpaFlags")).toUInt(), rsn = a.value(QStringLiteral("RsnFlags")).toUInt();
                const bool enterprise = (wpa | rsn) & 0x200;   // NM_802_11_AP_SEC_KEY_MGMT_802_1X
                const bool isActive = ap.path() == activeAp;
                if (isActive)
                    activeSsid = ssid;
                if (bySsid.contains(ssid) && bySsid[ssid].value(QStringLiteral("strength")).toInt() >= strength && !isActive)
                    continue;
                bySsid.insert(ssid, {
                    { QStringLiteral("ssid"), ssid },
                    { QStringLiteral("strength"), strength },
                    { QStringLiteral("secured"), (a.value(QStringLiteral("Flags")).toUInt() & 0x1) || wpa || rsn },
                    { QStringLiteral("enterprise"), enterprise },
                    { QStringLiteral("frequency"), a.value(QStringLiteral("Frequency")).toUInt() },
                    { QStringLiteral("active"), isActive || bySsid.value(ssid).value(QStringLiteral("active")).toBool() },
                    { QStringLiteral("saved"), savedBySsid.contains(ssid) },
                    { QStringLiteral("uuid"), savedBySsid.value(ssid) },
                });
            }
            QList<QVariantMap> aps = bySsid.values();
            std::sort(aps.begin(), aps.end(), [](const QVariantMap &a, const QVariantMap &b) {
                if (a.value(QStringLiteral("active")).toBool() != b.value(QStringLiteral("active")).toBool())
                    return a.value(QStringLiteral("active")).toBool();
                return a.value(QStringLiteral("strength")).toInt() > b.value(QStringLiteral("strength")).toInt();
            });
            for (const QVariantMap &ap : aps)
                m_accessPoints << ap;
        }
        m_devices << entry;
    }
    emit changed();
}

void NetworkConfig::listInterfacesOnly()
{
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (iface.type() == QNetworkInterface::Loopback)
            continue;
        QStringList v4, v6;
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QString text = entry.ip().toString() + QLatin1Char('/') + QString::number(entry.prefixLength());
            (entry.ip().protocol() == QAbstractSocket::IPv4Protocol ? v4 : v6) << text;
        }
        const bool up = iface.flags().testFlag(QNetworkInterface::IsRunning);
        m_devices << QVariantMap {
            { QStringLiteral("path"), QString() },
            { QStringLiteral("interface"), iface.name() },
            { QStringLiteral("type"), iface.type() == QNetworkInterface::Wifi ? QStringLiteral("Wi-Fi") : QStringLiteral("Ethernet") },
            { QStringLiteral("state"), up ? QStringLiteral("Connected") : QStringLiteral("Disconnected") },
            { QStringLiteral("connected"), up },
            { QStringLiteral("connection"), QString() },
            { QStringLiteral("mac"), iface.hardwareAddress() },
            { QStringLiteral("ipv4"), v4 },
            { QStringLiteral("ipv6"), v6 },
            { QStringLiteral("managed"), false },
        };
    }
}

void NetworkConfig::managerCall(const QString &path, const QString &interface, const QString &method,
                                const QVariantList &args, const QString &failure, const QString &success)
{
    ++m_busy;
    emit busyChanged();
    Platform::callAsync(this, bus(), Service, path, interface, method, args,
                        [this, failure, success](const QString &error, const QDBusMessage &) {
                            --m_busy;
                            emit busyChanged();
                            if (!error.isEmpty())
                                emit errorOccurred(failure + QStringLiteral(": ") + error);
                            else if (!success.isEmpty())
                                emit message(success);
                            // Give NetworkManager a moment to move the device along.
                            QTimer::singleShot(600, this, &NetworkConfig::refresh);
                        });
}

void NetworkConfig::setNetworkingEnabled(bool enabled)
{
    managerCall(ManagerPath, ManagerIface, QStringLiteral("Enable"), { enabled },
                enabled ? QStringLiteral("Could not turn networking on") : QStringLiteral("Could not turn networking off"));
}

void NetworkConfig::setWirelessEnabled(bool enabled)
{
    ++m_busy;
    emit busyChanged();
    Platform::setPropertyAsync(this, bus(), Service, ManagerPath, ManagerIface, QStringLiteral("WirelessEnabled"), enabled,
                               [this](const QString &error, const QDBusMessage &) {
                                   --m_busy;
                                   emit busyChanged();
                                   if (!error.isEmpty())
                                       emit errorOccurred(QStringLiteral("Could not switch Wi-Fi: ") + error);
                                   QTimer::singleShot(600, this, &NetworkConfig::refresh);
                               });
}

void NetworkConfig::scan()
{
    if (m_wifiDevice.isEmpty())
        return;
    managerCall(m_wifiDevice, WirelessIface, QStringLiteral("RequestScan"), { QVariantMap() },
                QStringLiteral("Could not scan for networks"));
    // Results trickle in over a few seconds.
    QTimer::singleShot(3500, this, &NetworkConfig::refresh);
}

void NetworkConfig::connectToNetwork(const QString &ssid, const QString &password)
{
    if (m_wifiDevice.isEmpty())
        return;
    for (const QVariant &v : m_accessPoints) {
        const QVariantMap ap = v.toMap();
        if (ap.value(QStringLiteral("ssid")).toString() != ssid)
            continue;
        if (ap.value(QStringLiteral("saved")).toBool() && password.isEmpty()) {
            activateConnection(ap.value(QStringLiteral("uuid")).toString());
            return;
        }
        if (ap.value(QStringLiteral("enterprise")).toBool()) {
            emit errorOccurred(QStringLiteral("%1 uses enterprise (802.1X) authentication, which the Control Center cannot set up yet.").arg(ssid));
            return;
        }
        NMSettings settings;
        settings[QStringLiteral("connection")] = { { QStringLiteral("id"), ssid }, { QStringLiteral("type"), QStringLiteral("802-11-wireless") } };
        settings[QStringLiteral("802-11-wireless")] = { { QStringLiteral("ssid"), ssid.toUtf8() }, { QStringLiteral("mode"), QStringLiteral("infrastructure") } };
        if (ap.value(QStringLiteral("secured")).toBool()) {
            if (password.isEmpty()) {
                emit errorOccurred(QStringLiteral("%1 needs a password.").arg(ssid));
                return;
            }
            settings[QStringLiteral("802-11-wireless-security")] = { { QStringLiteral("key-mgmt"), QStringLiteral("wpa-psk") },
                                                                     { QStringLiteral("psk"), password } };
        }
        managerCall(ManagerPath, ManagerIface, QStringLiteral("AddAndActivateConnection"),
                    { QVariant::fromValue(settings), QVariant::fromValue(QDBusObjectPath(m_wifiDevice)), QVariant::fromValue(QDBusObjectPath(QStringLiteral("/"))) },
                    QStringLiteral("Could not connect to %1").arg(ssid), QStringLiteral("Connecting to %1…").arg(ssid));
        return;
    }
    emit errorOccurred(QStringLiteral("%1 is out of range.").arg(ssid));
}

QString NetworkConfig::connectionPath(const QString &uuid) const
{
    for (const QVariant &v : m_connections) {
        if (v.toMap().value(QStringLiteral("uuid")).toString() == uuid)
            return v.toMap().value(QStringLiteral("path")).toString();
    }
    return {};
}

void NetworkConfig::activateConnection(const QString &uuid)
{
    const QString path = connectionPath(uuid);
    if (path.isEmpty())
        return;
    managerCall(ManagerPath, ManagerIface, QStringLiteral("ActivateConnection"),
                { QVariant::fromValue(QDBusObjectPath(path)), QVariant::fromValue(QDBusObjectPath(QStringLiteral("/"))),
                  QVariant::fromValue(QDBusObjectPath(QStringLiteral("/"))) },
                QStringLiteral("Could not connect"));
}

void NetworkConfig::disconnectDevice(const QString &devicePath)
{
    managerCall(devicePath, DeviceIface, QStringLiteral("Disconnect"), {}, QStringLiteral("Could not disconnect"));
}

void NetworkConfig::forgetConnection(const QString &uuid)
{
    const QString path = connectionPath(uuid);
    if (!path.isEmpty())
        managerCall(path, ConnectionIface, QStringLiteral("Delete"), {}, QStringLiteral("Could not forget the network"));
}
