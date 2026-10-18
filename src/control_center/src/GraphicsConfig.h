#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// Graphics page: the Vulkan devices on this machine, the one the Noir stack
// renders with and why, and the session settings that steer it (preferred
// GPU, NoirWM's compositor, validation). Those live in session.conf and take
// effect at the next login.
class GraphicsConfig : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)
    Q_PROPERTY(QString activeDevice READ activeDevice NOTIFY changed)
    Q_PROPERTY(QString activeReason READ activeReason NOTIFY changed)
    Q_PROPERTY(QString preferredDevice READ preferredDevice NOTIFY changed)
    Q_PROPERTY(bool compositorEnabled READ compositorEnabled NOTIFY changed)
    Q_PROPERTY(bool validationEnabled READ validationEnabled NOTIFY changed)
    Q_PROPERTY(bool validationAvailable READ validationAvailable CONSTANT)
    Q_PROPERTY(QString loaderVersion READ loaderVersion CONSTANT)
    Q_PROPERTY(QString sessionConfPath READ sessionConfPath CONSTANT)

public:
    explicit GraphicsConfig(QObject *parent = nullptr);

    // Every Vulkan device: name, type, typeName, vendor, apiVersion,
    // driverName, driverInfo, deviceId, presents (can draw to this X server),
    // active (the one the Noir stack picks).
    static QVariantList enumerateDevices(QString *activeName = nullptr, QString *activeReason = nullptr);

    QVariantList devices() const { return m_devices; }
    QString activeDevice() const { return m_activeDevice; }
    QString activeReason() const { return m_activeReason; }
    QString preferredDevice() const { return m_preferred; }
    bool compositorEnabled() const { return m_compositor; }
    bool validationEnabled() const { return m_validation; }
    bool validationAvailable() const;
    QString loaderVersion() const;
    QString sessionConfPath() const;

    Q_INVOKABLE void refresh();
    // A device name, or "" for automatic selection.
    Q_INVOKABLE bool setPreferredDevice(const QString &name);
    Q_INVOKABLE bool setCompositorEnabled(bool enabled);
    Q_INVOKABLE bool setValidationEnabled(bool enabled);

signals:
    void changed();
    void errorOccurred(const QString &message);

private:
    QVariantList m_devices;
    QString m_activeDevice;
    QString m_activeReason;
    QString m_preferred;
    bool m_compositor = true;
    bool m_validation = false;
};
