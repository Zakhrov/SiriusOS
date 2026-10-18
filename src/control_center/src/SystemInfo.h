#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// "About this system": the operating system, the Noir stack, and the
// hardware it runs on (processor, memory, graphics, displays, storage).
class SystemInfo : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString osName READ osName CONSTANT)
    Q_PROPERTY(QString osVersion READ osVersion CONSTANT)
    Q_PROPERTY(QString kernel READ kernel CONSTANT)
    Q_PROPERTY(QString architecture READ architecture CONSTANT)
    Q_PROPERTY(QString hostName READ hostName CONSTANT)
    Q_PROPERTY(QString noirVersion READ noirVersion CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    Q_PROPERTY(QString windowSystem READ windowSystem CONSTANT)
    Q_PROPERTY(QString cpuModel READ cpuModel CONSTANT)
    Q_PROPERTY(int cpuCores READ cpuCores CONSTANT)
    Q_PROPERTY(int cpuThreads READ cpuThreads CONSTANT)
    Q_PROPERTY(QString memory READ memory CONSTANT)
    Q_PROPERTY(QString machine READ machine CONSTANT)
    Q_PROPERTY(QString virtualization READ virtualization CONSTANT)
    Q_PROPERTY(QVariantList gpus READ gpus CONSTANT)
    Q_PROPERTY(QVariantList screens READ screens CONSTANT)
    Q_PROPERTY(QVariantList disks READ disks CONSTANT)
    Q_PROPERTY(QString uptime READ uptime NOTIFY uptimeChanged)

public:
    explicit SystemInfo(QObject *parent = nullptr);

    QString osName() const { return m_osName; }
    QString osVersion() const { return m_osVersion; }
    QString kernel() const;
    QString architecture() const;
    QString hostName() const;
    QString noirVersion() const;
    QString qtVersion() const;
    QString windowSystem() const { return m_windowSystem; }
    QString cpuModel() const { return m_cpuModel; }
    int cpuCores() const { return m_cpuCores; }
    int cpuThreads() const { return m_cpuThreads; }
    QString memory() const { return m_memory; }
    QString machine() const { return m_machine; }
    QString virtualization() const { return m_virtualization; }
    QVariantList gpus() const { return m_gpus; }
    QVariantList screens() const;
    QVariantList disks() const;
    QString uptime() const;

    // A plain-text summary, for bug reports.
    Q_INVOKABLE QString summary() const;
    Q_INVOKABLE void copySummary() const;

    static QString formatBytes(qint64 bytes);

signals:
    void uptimeChanged();

private:
    QString m_osName;
    QString m_osVersion;
    QString m_windowSystem;
    QString m_cpuModel;
    int m_cpuCores = 0;
    int m_cpuThreads = 0;
    QString m_memory;
    QString m_machine;
    QString m_virtualization;
    QVariantList m_gpus;
};
