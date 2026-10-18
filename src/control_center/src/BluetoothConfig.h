#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// Bluetooth page, through BlueZ: the adapter's power and discovery, known
// and nearby devices, pairing, connecting and removing them.
class BluetoothConfig : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString adapterName READ adapterName NOTIFY changed)
    Q_PROPERTY(QString adapterAddress READ adapterAddress NOTIFY changed)
    Q_PROPERTY(bool powered READ powered NOTIFY changed)
    Q_PROPERTY(bool discovering READ discovering NOTIFY changed)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit BluetoothConfig(QObject *parent = nullptr);
    ~BluetoothConfig() override;

    bool available() const { return !m_adapter.isEmpty(); }
    QString status() const { return m_status; }
    QString adapterName() const { return m_adapterName; }
    QString adapterAddress() const { return m_adapterAddress; }
    bool powered() const { return m_powered; }
    bool discovering() const { return m_discovering; }
    QVariantList devices() const { return m_devices; }
    bool busy() const { return m_busy > 0; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setPowered(bool powered);
    Q_INVOKABLE void setDiscovering(bool discovering);
    Q_INVOKABLE void pair(const QString &path);
    Q_INVOKABLE void connectDevice(const QString &path);
    Q_INVOKABLE void disconnectDevice(const QString &path);
    Q_INVOKABLE void removeDevice(const QString &path);

signals:
    void changed();
    void busyChanged();
    void errorOccurred(const QString &message);

private:
    void deviceCall(const QString &path, const QString &interface, const QString &method,
                    const QVariantList &args, const QString &failure);

    QString m_adapter;          // object path
    QString m_status;
    QString m_adapterName;
    QString m_adapterAddress;
    bool m_powered = false;
    bool m_discovering = false;
    bool m_startedDiscovery = false;
    QVariantList m_devices;
    int m_busy = 0;
};
