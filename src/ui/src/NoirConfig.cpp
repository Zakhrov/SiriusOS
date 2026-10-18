#include "NoirConfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace {
NoirConfig *s_instance = nullptr;
}

NoirConfig::NoirConfig(QObject *parent)
    : QObject(parent)
    , m_settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("SiriusOS"), QStringLiteral("Noir"))
{
    s_instance = this;
    QDir().mkpath(QFileInfo(m_settings.fileName()).absolutePath());
    // QSettings saves by writing a new file and renaming it over the old one,
    // which drops the file from the watcher; the directory watch notices the
    // new file and watch() picks it up again.
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &NoirConfig::reload);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &NoirConfig::reload);
    watch();
}

NoirConfig *NoirConfig::instance()
{
    if (!s_instance)
        s_instance = new NoirConfig(QCoreApplication::instance());
    return s_instance;
}

NoirConfig *NoirConfig::create(QQmlEngine *, QJSEngine *)
{
    NoirConfig *self = instance();
    QJSEngine::setObjectOwnership(self, QJSEngine::CppOwnership);
    return self;
}

QVariant NoirConfig::value(const QString &key, const QVariant &fallback) const
{
    return m_settings.value(key, fallback);
}

void NoirConfig::setValue(const QString &key, const QVariant &value)
{
    store(key, value);
}

void NoirConfig::remove(const QString &key)
{
    m_settings.remove(key);
    m_settings.sync();
    emit changed();
}

QStringList NoirConfig::childGroups(const QString &group) const
{
    auto &settings = const_cast<QSettings &>(m_settings);
    settings.beginGroup(group);
    const QStringList groups = settings.childGroups();
    settings.endGroup();
    return groups;
}

QString NoirConfig::timeFormat(bool withSeconds) const
{
    QString format = clock24Hour() ? QStringLiteral("hh:mm") : QStringLiteral("h:mm");
    if (withSeconds)
        format += QStringLiteral(":ss");
    if (!clock24Hour())
        format += QStringLiteral(" AP");
    return format;
}

void NoirConfig::store(const QString &key, const QVariant &value)
{
    if (m_settings.value(key) == value)
        return;
    m_settings.setValue(key, value);
    m_settings.sync();
    emit changed();
}

void NoirConfig::reload()
{
    m_settings.sync();
    watch();
    emit changed();
}

void NoirConfig::watch()
{
    const QString file = m_settings.fileName();
    const QString dir = QFileInfo(file).absolutePath();
    if (!m_watcher.directories().contains(dir))
        m_watcher.addPath(dir);
    if (QFileInfo::exists(file) && !m_watcher.files().contains(file))
        m_watcher.addPath(file);
}
