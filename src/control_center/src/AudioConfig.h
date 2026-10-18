#pragma once

#include <QObject>
#include <QProcess>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantList>

// Sound page, through the PulseAudio protocol (PulseAudio or PipeWire's
// pipewire-pulse) with pactl: output and input devices with their volume,
// mute and default, plus the applications playing sound. `pactl subscribe`
// keeps everything current while the page is open.
class AudioConfig : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString serverName READ serverName NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantList sinks READ sinks NOTIFY changed)
    Q_PROPERTY(QVariantList sources READ sources NOTIFY changed)
    Q_PROPERTY(QVariantList streams READ streams NOTIFY changed)
    Q_PROPERTY(bool live READ live WRITE setLive NOTIFY liveChanged)

public:
    explicit AudioConfig(QObject *parent = nullptr);
    ~AudioConfig() override;

    bool available() const { return m_available; }
    QString serverName() const { return m_serverName; }
    QString status() const { return m_status; }
    QVariantList sinks() const { return m_sinks; }
    QVariantList sources() const { return m_sources; }
    QVariantList streams() const { return m_streams; }
    bool live() const { return m_subscription.state() != QProcess::NotRunning; }
    void setLive(bool live);

    Q_INVOKABLE void refresh();
    // `kind` is "sink", "source" or "sink-input"; volume 1.0 is 100 %.
    Q_INVOKABLE void setVolume(const QString &kind, const QString &name, double volume);
    Q_INVOKABLE void setMuted(const QString &kind, const QString &name, bool muted);
    Q_INVOKABLE void setDefault(const QString &kind, const QString &name);
    Q_INVOKABLE void setPort(const QString &kind, const QString &name, const QString &port);

signals:
    void changed();
    void liveChanged();
    void errorOccurred(const QString &message);

private:
    void pactl(const QStringList &args);
    QVariantList list(const QString &what, const QString &defaultName) const;

    bool m_available = false;
    QString m_serverName;
    QString m_status;
    QVariantList m_sinks;
    QVariantList m_sources;
    QVariantList m_streams;
    QProcess m_subscription;
    QTimer m_debounce;
};
