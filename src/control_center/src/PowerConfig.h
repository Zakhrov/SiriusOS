#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// Power page: batteries (UPower), power profiles (power-profiles-daemon),
// screen blanking and power saving (X screen saver + DPMS, saved and applied
// again at login) and the session's suspend / restart / shut down (logind).
class PowerConfig : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool upowerAvailable READ upowerAvailable NOTIFY changed)
    Q_PROPERTY(bool onBattery READ onBattery NOTIFY changed)
    Q_PROPERTY(QVariantList batteries READ batteries NOTIFY changed)
    Q_PROPERTY(bool profilesAvailable READ profilesAvailable NOTIFY changed)
    Q_PROPERTY(QStringList profiles READ profiles NOTIFY changed)
    Q_PROPERTY(QString activeProfile READ activeProfile NOTIFY changed)
    Q_PROPERTY(int blankMinutes READ blankMinutes NOTIFY changed)
    Q_PROPERTY(int screenOffMinutes READ screenOffMinutes NOTIFY changed)
    Q_PROPERTY(bool dpmsAvailable READ dpmsAvailable NOTIFY changed)
    Q_PROPERTY(QVariantMap sessionActions READ sessionActions NOTIFY changed)

public:
    explicit PowerConfig(QObject *parent = nullptr);

    bool upowerAvailable() const { return m_upower; }
    bool onBattery() const { return m_onBattery; }
    QVariantList batteries() const { return m_batteries; }
    bool profilesAvailable() const { return !m_profileService.isEmpty(); }
    QStringList profiles() const { return m_profiles; }
    QString activeProfile() const { return m_activeProfile; }
    int blankMinutes() const { return m_blankMinutes; }
    int screenOffMinutes() const { return m_screenOffMinutes; }
    bool dpmsAvailable() const { return m_dpms; }
    // canSuspend, canHibernate, canReboot, canPowerOff: whether logind allows them.
    QVariantMap sessionActions() const { return m_actions; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setActiveProfile(const QString &profile);
    // 0 means never. Saved, and applied again at login by --apply.
    Q_INVOKABLE void setBlankMinutes(int minutes);
    Q_INVOKABLE void setScreenOffMinutes(int minutes);
    // "Suspend", "Hibernate", "Reboot" or "PowerOff".
    Q_INVOKABLE void sessionAction(const QString &action);

    // Applies the saved screen settings (login).
    static void applySaved();

signals:
    void changed();
    void errorOccurred(const QString &message);

private:
    static void applyScreen(int blankMinutes, int screenOffMinutes);

    bool m_upower = false;
    bool m_onBattery = false;
    QVariantList m_batteries;
    QString m_profileService;
    QString m_profilePath;
    QString m_profileIface;
    QStringList m_profiles;
    QString m_activeProfile;
    int m_blankMinutes = 0;
    int m_screenOffMinutes = 0;
    bool m_dpms = false;
    QVariantMap m_actions;
};
