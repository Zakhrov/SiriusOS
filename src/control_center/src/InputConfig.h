#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// Input Devices page. Keyboard: key repeat (XKB) and layouts (setxkbmap).
// Mice and touchpads: libinput options through XInput2 device properties,
// set for every device of a class at once. Everything is saved and applied
// again at login by --apply (and to devices plugged in later, on refresh).
class InputConfig : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool repeatEnabled READ repeatEnabled NOTIFY changed)
    Q_PROPERTY(int repeatDelay READ repeatDelay NOTIFY changed)
    Q_PROPERTY(int repeatRate READ repeatRate NOTIFY changed)
    Q_PROPERTY(QVariantList availableLayouts READ availableLayouts CONSTANT)
    Q_PROPERTY(QStringList layouts READ layouts NOTIFY changed)
    Q_PROPERTY(QString layoutSwitch READ layoutSwitch NOTIFY changed)
    Q_PROPERTY(bool layoutsSupported READ layoutsSupported CONSTANT)
    Q_PROPERTY(QVariantList pointers READ pointers NOTIFY changed)
    Q_PROPERTY(QVariantMap mouse READ mouse NOTIFY changed)
    Q_PROPERTY(QVariantMap touchpad READ touchpad NOTIFY changed)

public:
    explicit InputConfig(QObject *parent = nullptr);

    bool repeatEnabled() const { return m_repeatEnabled; }
    int repeatDelay() const { return m_repeatDelay; }
    int repeatRate() const { return m_repeatRate; }
    QVariantList availableLayouts() const;
    QStringList layouts() const { return m_layouts; }
    QString layoutSwitch() const { return m_layoutSwitch; }
    bool layoutsSupported() const;
    // {id, name, touchpad}
    QVariantList pointers() const { return m_pointers; }
    // The current settings of the class, as read from its first device:
    // accelSpeed (-1..1), flatProfile, naturalScroll, leftHanded,
    // middleEmulation, and for touchpads tapping and disableWhileTyping.
    // A key is missing when no device of the class supports it.
    QVariantMap mouse() const { return m_mouse; }
    QVariantMap touchpad() const { return m_touchpad; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setRepeat(bool enabled, int delayMs, int ratePerSecond);
    Q_INVOKABLE void setLayouts(const QStringList &layouts, const QString &switchOption);
    Q_INVOKABLE void setPointerSetting(bool touchpad, const QString &key, const QVariant &value);

    static void applySaved();

signals:
    void changed();
    void errorOccurred(const QString &message);

private:
    static bool applyRepeat(bool enabled, int delayMs, int ratePerSecond);
    static QString applyLayouts(const QStringList &layouts, const QString &switchOption);
    static void applyPointer(bool touchpad, const QString &key, const QVariant &value);

    bool m_repeatEnabled = true;
    int m_repeatDelay = 600;
    int m_repeatRate = 25;
    QStringList m_layouts;
    QString m_layoutSwitch;
    QVariantList m_pointers;
    QVariantMap m_mouse;
    QVariantMap m_touchpad;
};
