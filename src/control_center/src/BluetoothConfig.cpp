#include "BluetoothConfig.h"

#include "Platform.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QTimer>

#include <algorithm>

using InterfaceMap = QMap<QString, QVariantMap>;
using ManagedObjects = QMap<QDBusObjectPath, InterfaceMap>;
Q_DECLARE_METATYPE(InterfaceMap)
Q_DECLARE_METATYPE(ManagedObjects)

namespace {

const QString Service = QStringLiteral("org.bluez");
const QString AdapterIface = QStringLiteral("org.bluez.Adapter1");
const QString DeviceIface = QStringLiteral("org.bluez.Device1");

QDBusConnection bus() { return QDBusConnection::systemBus(); }

QString kindOf(const QString &icon)
{
    if (icon.startsWith(QLatin1String("audio"))) return QStringLiteral("Audio");
    if (icon.startsWith(QLatin1String("input-keyboard"))) return QStringLiteral("Keyboard");
    if (icon.startsWith(QLatin1String("input-mouse"))) return QStringLiteral("Mouse");
    if (icon.startsWith(QLatin1String("input-gaming"))) return QStringLiteral("Game controller");
    if (icon.startsWith(QLatin1String("input"))) return QStringLiteral("Input device");
    if (icon.startsWith(QLatin1String("phone"))) return QStringLiteral("Phone");
    if (icon.startsWith(QLatin1String("computer"))) return QStringLiteral("Computer");
    return QStringLiteral("Device");
}

} // namespace

BluetoothConfig::BluetoothConfig(QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<InterfaceMap>();
    qDBusRegisterMetaType<ManagedObjects>();
    refresh();
}

BluetoothConfig::~BluetoothConfig()
{
    // Don't leave the adapter scanning after the window closes.
    if (m_startedDiscovery && !m_adapter.isEmpty())
        Platform::call(bus(), Service, m_adapter, AdapterIface, QStringLiteral("StopDiscovery"), {}, 500);
}

void BluetoothConfig::refresh()
{
    m_adapter.clear();
    m_devices.clear();
    if (!bus().isConnected() || !Platform::hasService(bus(), Service)) {
        m_status = QStringLiteral("The Bluetooth service (BlueZ) is not running.");
        emit changed();
        return;
    }
    const QDBusMessage reply = Platform::call(bus(), Service, QStringLiteral("/"),
                                              QStringLiteral("org.freedesktop.DBus.ObjectManager"), QStringLiteral("GetManagedObjects"));
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        m_status = QStringLiteral("The Bluetooth service did not answer.");
        emit changed();
        return;
    }
    const ManagedObjects objects = qdbus_cast<ManagedObjects>(reply.arguments().first());
    for (auto it = objects.begin(); it != objects.end(); ++it) {
        if (!it.value().contains(AdapterIface) || !m_adapter.isEmpty())
            continue;
        const QVariantMap a = it.value().value(AdapterIface);
        m_adapter = it.key().path();
        m_adapterName = a.value(QStringLiteral("Alias")).toString();
        m_adapterAddress = a.value(QStringLiteral("Address")).toString();
        m_powered = a.value(QStringLiteral("Powered")).toBool();
        m_discovering = a.value(QStringLiteral("Discovering")).toBool();
    }
    m_status = m_adapter.isEmpty() ? QStringLiteral("No Bluetooth adapter found.") : QString();

    QList<QVariantMap> devices;
    for (auto it = objects.begin(); it != objects.end(); ++it) {
        if (!it.value().contains(DeviceIface))
            continue;
        const QVariantMap d = it.value().value(DeviceIface);
        if (d.value(QStringLiteral("Adapter")).value<QDBusObjectPath>().path() != m_adapter)
            continue;
        const QString icon = d.value(QStringLiteral("Icon")).toString();
        QString name = d.value(QStringLiteral("Alias")).toString();
        if (name.isEmpty())
            name = d.value(QStringLiteral("Address")).toString();
        const QVariantMap battery = it.value().value(QStringLiteral("org.bluez.Battery1"));
        devices << QVariantMap {
            { QStringLiteral("path"), it.key().path() },
            { QStringLiteral("name"), name },
            { QStringLiteral("address"), d.value(QStringLiteral("Address")) },
            { QStringLiteral("kind"), kindOf(icon) },
            { QStringLiteral("paired"), d.value(QStringLiteral("Paired")) },
            { QStringLiteral("connected"), d.value(QStringLiteral("Connected")) },
            { QStringLiteral("trusted"), d.value(QStringLiteral("Trusted")) },
            { QStringLiteral("rssi"), d.contains(QStringLiteral("RSSI")) ? d.value(QStringLiteral("RSSI")) : QVariant() },
            { QStringLiteral("battery"), battery.contains(QStringLiteral("Percentage")) ? battery.value(QStringLiteral("Percentage")) : QVariant() },
        };
    }
    // Connected, then paired, then by name.
    std::sort(devices.begin(), devices.end(), [](const QVariantMap &a, const QVariantMap &b) {
        auto rank = [](const QVariantMap &d) {
            return d.value(QStringLiteral("connected")).toBool() ? 0 : d.value(QStringLiteral("paired")).toBool() ? 1 : 2;
        };
        if (rank(a) != rank(b)) return rank(a) < rank(b);
        return a.value(QStringLiteral("name")).toString().localeAwareCompare(b.value(QStringLiteral("name")).toString()) < 0;
    });
    for (const QVariantMap &d : devices)
        m_devices << d;
    emit changed();
}

void BluetoothConfig::deviceCall(const QString &path, const QString &interface, const QString &method,
                                 const QVariantList &args, const QString &failure)
{
    ++m_busy;
    emit busyChanged();
    Platform::callAsync(this, bus(), Service, path, interface, method, args,
                        [this, failure](const QString &error, const QDBusMessage &) {
                            --m_busy;
                            emit busyChanged();
                            if (!error.isEmpty())
                                emit errorOccurred(failure + QStringLiteral(": ") + error);
                            refresh();
                        });
}

void BluetoothConfig::setPowered(bool powered)
{
    if (m_adapter.isEmpty())
        return;
    ++m_busy;
    emit busyChanged();
    Platform::setPropertyAsync(this, bus(), Service, m_adapter, AdapterIface, QStringLiteral("Powered"), powered,
                               [this](const QString &error, const QDBusMessage &) {
                                   --m_busy;
                                   emit busyChanged();
                                   if (!error.isEmpty())
                                       emit errorOccurred(QStringLiteral("Could not switch Bluetooth (is it blocked by rfkill?): ") + error);
                                   refresh();
                               });
}

void BluetoothConfig::setDiscovering(bool discovering)
{
    if (m_adapter.isEmpty() || discovering == m_discovering)
        return;
    m_startedDiscovery = discovering;
    deviceCall(m_adapter, AdapterIface, discovering ? QStringLiteral("StartDiscovery") : QStringLiteral("StopDiscovery"), {},
               discovering ? QStringLiteral("Could not search for devices") : QStringLiteral("Could not stop searching"));
}

void BluetoothConfig::pair(const QString &path)
{
    ++m_busy;
    emit busyChanged();
    Platform::callAsync(this, bus(), Service, path, DeviceIface, QStringLiteral("Pair"), {},
                        [this, path](const QString &error, const QDBusMessage &) {
                            --m_busy;
                            emit busyChanged();
                            if (!error.isEmpty()) {
                                emit errorOccurred(QStringLiteral("Pairing failed: ") + error);
                                refresh();
                                return;
                            }
                            // Trust it so it may reconnect by itself, then connect.
                            Platform::setPropertyAsync(this, bus(), Service, path, DeviceIface, QStringLiteral("Trusted"), true, nullptr);
                            connectDevice(path);
                        });
}

void BluetoothConfig::connectDevice(const QString &path)
{
    deviceCall(path, DeviceIface, QStringLiteral("Connect"), {}, QStringLiteral("Could not connect"));
}

void BluetoothConfig::disconnectDevice(const QString &path)
{
    deviceCall(path, DeviceIface, QStringLiteral("Disconnect"), {}, QStringLiteral("Could not disconnect"));
}

void BluetoothConfig::removeDevice(const QString &path)
{
    deviceCall(m_adapter, AdapterIface, QStringLiteral("RemoveDevice"), { QVariant::fromValue(QDBusObjectPath(path)) },
               QStringLiteral("Could not remove the device"));
}
