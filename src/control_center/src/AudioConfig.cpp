#include "AudioConfig.h"

#include "Platform.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>

namespace {

// Average of the channel volumes; pactl reports raw values where 65536 is 100 %.
double averageVolume(const QJsonObject &volume)
{
    double sum = 0;
    int channels = 0;
    for (auto it = volume.begin(); it != volume.end(); ++it) {
        sum += it.value().toObject().value(QStringLiteral("value")).toDouble();
        ++channels;
    }
    return channels ? sum / channels / 65536.0 : 0.0;
}

} // namespace

AudioConfig::AudioConfig(QObject *parent)
    : QObject(parent)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(120);
    connect(&m_debounce, &QTimer::timeout, this, &AudioConfig::refresh);
    connect(&m_subscription, &QProcess::readyReadStandardOutput, this, [this] {
        m_subscription.readAllStandardOutput();
        m_debounce.start();
    });
    connect(&m_subscription, &QProcess::stateChanged, this, &AudioConfig::liveChanged);
    refresh();
}

AudioConfig::~AudioConfig()
{
    setLive(false);
}

void AudioConfig::setLive(bool live)
{
    if (live == this->live())
        return;
    if (live) {
        m_subscription.start(QStringLiteral("pactl"), { QStringLiteral("subscribe") });
    } else {
        m_subscription.kill();
        m_subscription.waitForFinished(500);
    }
}

void AudioConfig::refresh()
{
    QString error;
    const QString info = Platform::run(QStringLiteral("pactl"), { QStringLiteral("-f"), QStringLiteral("json"), QStringLiteral("info") }, &error);
    if (info.isNull()) {
        m_available = false;
        m_status = error.contains(QLatin1String("not installed"))
            ? QStringLiteral("pactl is not installed (pulseaudio-utils).")
            : QStringLiteral("No sound server is running (PulseAudio or PipeWire).");
        m_sinks.clear();
        m_sources.clear();
        m_streams.clear();
        emit changed();
        return;
    }
    const QJsonObject server = QJsonDocument::fromJson(info.toUtf8()).object();
    m_available = true;
    m_status.clear();
    m_serverName = server.value(QStringLiteral("server_name")).toString() + QLatin1Char(' ')
                 + server.value(QStringLiteral("server_version")).toString();
    m_sinks = list(QStringLiteral("sinks"), server.value(QStringLiteral("default_sink_name")).toString());
    m_sources = list(QStringLiteral("sources"), server.value(QStringLiteral("default_source_name")).toString());
    m_streams = list(QStringLiteral("sink-inputs"), QString());
    emit changed();
}

QVariantList AudioConfig::list(const QString &what, const QString &defaultName) const
{
    QVariantList result;
    const QString out = Platform::run(QStringLiteral("pactl"), { QStringLiteral("-f"), QStringLiteral("json"), QStringLiteral("list"), what });
    const QJsonArray items = QJsonDocument::fromJson(out.toUtf8()).array();
    for (const QJsonValue &value : items) {
        const QJsonObject item = value.toObject();
        const QJsonObject properties = item.value(QStringLiteral("properties")).toObject();
        // Monitor sources mirror an output; they are not microphones.
        if (what == QLatin1String("sources")
            && properties.value(QStringLiteral("device.class")).toString() == QLatin1String("monitor"))
            continue;
        QVariantMap entry;
        if (what == QLatin1String("sink-inputs")) {
            entry[QStringLiteral("name")] = QString::number(item.value(QStringLiteral("index")).toInt());
            QString app = properties.value(QStringLiteral("application.name")).toString();
            if (app.isEmpty())
                app = properties.value(QStringLiteral("media.name")).toString();
            entry[QStringLiteral("description")] = app;
            entry[QStringLiteral("detail")] = properties.value(QStringLiteral("media.name")).toString();
            entry[QStringLiteral("icon")] = properties.value(QStringLiteral("application.icon_name")).toString();
        } else {
            const QString name = item.value(QStringLiteral("name")).toString();
            entry[QStringLiteral("name")] = name;
            entry[QStringLiteral("description")] = item.value(QStringLiteral("description")).toString();
            entry[QStringLiteral("isDefault")] = name == defaultName;
            QVariantList ports;
            for (const QJsonValue &port : item.value(QStringLiteral("ports")).toArray()) {
                const QJsonObject p = port.toObject();
                if (p.value(QStringLiteral("availability")).toString() == QLatin1String("not available"))
                    continue;
                ports << QVariantMap { { QStringLiteral("name"), p.value(QStringLiteral("name")).toString() },
                                       { QStringLiteral("text"), p.value(QStringLiteral("description")).toString() } };
            }
            entry[QStringLiteral("ports")] = ports;
            entry[QStringLiteral("activePort")] = item.value(QStringLiteral("active_port")).toString();
        }
        entry[QStringLiteral("volume")] = std::round(averageVolume(item.value(QStringLiteral("volume")).toObject()) * 100) / 100;
        entry[QStringLiteral("muted")] = item.value(QStringLiteral("mute")).toBool();
        result << entry;
    }
    return result;
}

void AudioConfig::pactl(const QStringList &args)
{
    QString error;
    if (Platform::run(QStringLiteral("pactl"), args, &error).isNull())
        emit errorOccurred(error);
    if (!live())
        refresh();
}

void AudioConfig::setVolume(const QString &kind, const QString &name, double volume)
{
    const int percent = int(std::lround(std::clamp(volume, 0.0, 1.5) * 100));
    pactl({ QStringLiteral("set-%1-volume").arg(kind), name, QStringLiteral("%1%").arg(percent) });
}

void AudioConfig::setMuted(const QString &kind, const QString &name, bool muted)
{
    pactl({ QStringLiteral("set-%1-mute").arg(kind), name, muted ? QStringLiteral("1") : QStringLiteral("0") });
}

void AudioConfig::setDefault(const QString &kind, const QString &name)
{
    pactl({ QStringLiteral("set-default-%1").arg(kind), name });
}

void AudioConfig::setPort(const QString &kind, const QString &name, const QString &port)
{
    pactl({ QStringLiteral("set-%1-port").arg(kind), name, port });
}
