#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// Network page, through NetworkManager: devices and their addresses,
// networking and Wi-Fi switches, Wi-Fi networks (scan, connect with a
// password, disconnect, forget). Without NetworkManager (wicked, systemd-
// networkd, ...) the interfaces are listed read-only.
class NetworkConfig : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool managerAvailable READ managerAvailable NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool networkingEnabled READ networkingEnabled NOTIFY changed)
    Q_PROPERTY(bool wirelessEnabled READ wirelessEnabled NOTIFY changed)
    Q_PROPERTY(bool wirelessHardwareEnabled READ wirelessHardwareEnabled NOTIFY changed)
    Q_PROPERTY(bool hasWifi READ hasWifi NOTIFY changed)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)
    Q_PROPERTY(QVariantList accessPoints READ accessPoints NOTIFY changed)
    Q_PROPERTY(QVariantList connections READ connections NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit NetworkConfig(QObject *parent = nullptr);

    bool managerAvailable() const { return m_available; }
    QString status() const { return m_status; }
    bool networkingEnabled() const { return m_networking; }
    bool wirelessEnabled() const { return m_wireless; }
    bool wirelessHardwareEnabled() const { return m_wirelessHardware; }
    bool hasWifi() const { return !m_wifiDevice.isEmpty(); }
    QVariantList devices() const { return m_devices; }
    QVariantList accessPoints() const { return m_accessPoints; }
    QVariantList connections() const { return m_connections; }
    bool busy() const { return m_busy > 0; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setNetworkingEnabled(bool enabled);
    Q_INVOKABLE void setWirelessEnabled(bool enabled);
    Q_INVOKABLE void scan();
    // A saved network connects with its saved settings; otherwise a new
    // connection is made (open, or WPA/WPA2/WPA3 personal with `password`).
    Q_INVOKABLE void connectToNetwork(const QString &ssid, const QString &password);
    Q_INVOKABLE void activateConnection(const QString &uuid);
    Q_INVOKABLE void disconnectDevice(const QString &devicePath);
    Q_INVOKABLE void forgetConnection(const QString &uuid);

signals:
    void changed();
    void busyChanged();
    void errorOccurred(const QString &message);
    void message(const QString &text);

private:
    void listInterfacesOnly();
    void managerCall(const QString &path, const QString &interface, const QString &method,
                     const QVariantList &args, const QString &failure, const QString &success = {});
    QString connectionPath(const QString &uuid) const;

    bool m_available = false;
    QString m_status;
    bool m_networking = false;
    bool m_wireless = false;
    bool m_wirelessHardware = false;
    QString m_wifiDevice;      // object path of the first Wi-Fi device
    QVariantList m_devices;
    QVariantList m_accessPoints;
    QVariantList m_connections;
    int m_busy = 0;
};
