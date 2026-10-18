#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QQmlEngine>
#include <QSettings>
#include <QVariant>

// Desktop settings shared by every Noir program, stored in
// ~/.config/SiriusOS/Noir.conf. The Control Center writes them; the shell
// and anything else reading them follow changes live, because the file is
// watched and every instance re-reads it when another process saves it.
//
// Settings the shell binds to have typed properties. Hardware settings that
// the Control Center applies itself (displays, input devices, screen blanking)
// go through value() / setValue() under their own groups.
class NoirConfig : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

#define NOIR_CONFIG_PROPERTY(Type, name, setter, key, fallback)                                \
    Q_PROPERTY(Type name READ name WRITE setter NOTIFY changed)                                 \
public:                                                                                         \
    Type name() const { return m_settings.value(QStringLiteral(key), fallback).value<Type>(); } \
    void setter(const Type &value) { store(QStringLiteral(key), QVariant::fromValue(value)); }  \
private:

    // Appearance
    NOIR_CONFIG_PROPERTY(bool, reducedMotion, setReducedMotion, "Appearance/ReducedMotion", false)
    NOIR_CONFIG_PROPERTY(double, animationScale, setAnimationScale, "Appearance/AnimationScale", 1.0)
    NOIR_CONFIG_PROPERTY(double, glassOpacity, setGlassOpacity, "Appearance/GlassOpacity", 0.6)
    NOIR_CONFIG_PROPERTY(bool, glassBlur, setGlassBlur, "Appearance/GlassBlur", true)
    NOIR_CONFIG_PROPERTY(bool, showDesktopIcons, setShowDesktopIcons, "Desktop/ShowIcons", true)
    NOIR_CONFIG_PROPERTY(bool, showDesktopWidgets, setShowDesktopWidgets, "Desktop/ShowWidgets", true)
    NOIR_CONFIG_PROPERTY(bool, showWallpaperGrid, setShowWallpaperGrid, "Desktop/ShowGrid", true)
    // Clock
    NOIR_CONFIG_PROPERTY(bool, clock24Hour, setClock24Hour, "Clock/TwentyFourHour", true)
    NOIR_CONFIG_PROPERTY(bool, clockShowSeconds, setClockShowSeconds, "Clock/ShowSeconds", true)
    NOIR_CONFIG_PROPERTY(bool, clockShowDate, setClockShowDate, "Clock/ShowDate", true)
    // Notifications
    NOIR_CONFIG_PROPERTY(bool, doNotDisturb, setDoNotDisturb, "Notifications/DoNotDisturb", false)
    NOIR_CONFIG_PROPERTY(int, notificationTimeout, setNotificationTimeout, "Notifications/TimeoutSeconds", 6)
    // Applications
    NOIR_CONFIG_PROPERTY(QString, terminal, setTerminal, "Applications/Terminal", QStringLiteral("x-terminal-emulator"))

#undef NOIR_CONFIG_PROPERTY

public:
    // No default argument: QML must go through create() and share instance().
    explicit NoirConfig(QObject *parent);

    static NoirConfig *instance();
    static NoirConfig *create(QQmlEngine *, QJSEngine *);

    QString fileName() const { return m_settings.fileName(); }

    Q_INVOKABLE QVariant value(const QString &key, const QVariant &fallback = {}) const;
    Q_INVOKABLE void setValue(const QString &key, const QVariant &value);
    Q_INVOKABLE void remove(const QString &key);
    Q_INVOKABLE QStringList childGroups(const QString &group) const;

    // Strftime-like patterns for Qt.formatTime, honouring the clock settings.
    Q_INVOKABLE QString timeFormat(bool withSeconds) const;

signals:
    void changed();

private:
    void store(const QString &key, const QVariant &value);
    void reload();
    void watch();

    QSettings m_settings;
    QFileSystemWatcher m_watcher;
};
