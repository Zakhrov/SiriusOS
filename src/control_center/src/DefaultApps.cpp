#include "DefaultApps.h"

#include "NoirConfig.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

namespace {

struct Category {
    const char *id;
    const char *title;
    const char *icon;
    QStringList types;      // the first one decides which applications qualify
};

const QList<Category> &categoryList()
{
    static const QList<Category> list = {
        { "browser", "Web browser", "globe", { QStringLiteral("x-scheme-handler/https"), QStringLiteral("x-scheme-handler/http"), QStringLiteral("text/html") } },
        { "files", "File manager", "folder", { QStringLiteral("inode/directory") } },
        { "email", "E-mail", "bell", { QStringLiteral("x-scheme-handler/mailto") } },
        { "text", "Text editor", "apps", { QStringLiteral("text/plain") } },
        { "images", "Image viewer", "display", { QStringLiteral("image/png"), QStringLiteral("image/jpeg"), QStringLiteral("image/gif"), QStringLiteral("image/webp") } },
        { "video", "Video player", "display", { QStringLiteral("video/mp4"), QStringLiteral("video/x-matroska"), QStringLiteral("video/webm") } },
        { "music", "Music player", "volume", { QStringLiteral("audio/mpeg"), QStringLiteral("audio/flac"), QStringLiteral("audio/ogg") } },
    };
    return list;
}

struct DesktopEntry {
    QString id;
    QString name;
    QString icon;
    QString exec;
    QStringList mimeTypes;
    QStringList categories;
};

QString cleanExec(const QString &exec)
{
    QString out;
    for (int i = 0; i < exec.size(); ++i) {
        if (exec.at(i) == QLatin1Char('%') && i + 1 < exec.size()) {
            if (exec.at(i + 1) == QLatin1Char('%'))
                out += QLatin1Char('%');
            ++i;
            continue;
        }
        out += exec.at(i);
    }
    return out.simplified();
}

// Every application's desktop entry, by id; user entries shadow system ones.
QList<DesktopEntry> desktopEntries()
{
    QList<DesktopEntry> entries;
    QSet<QString> seen;
    for (const QString &dir : QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation)) {
        QDirIterator it(dir, { QStringLiteral("*.desktop") }, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            QString id = QDir(dir).relativeFilePath(path);
            id.replace(QLatin1Char('/'), QLatin1Char('-'));
            if (seen.contains(id))
                continue;
            seen.insert(id);
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            DesktopEntry entry;
            entry.id = id;
            bool inMain = false, hidden = false, application = false;
            while (!file.atEnd()) {
                const QString line = QString::fromUtf8(file.readLine()).trimmed();
                if (line.startsWith(QLatin1Char('['))) {
                    inMain = line == QLatin1String("[Desktop Entry]");
                    continue;
                }
                if (!inMain)
                    continue;
                const QString key = line.section(QLatin1Char('='), 0, 0).trimmed();
                const QString value = line.section(QLatin1Char('='), 1).trimmed();
                if (key == QLatin1String("Name")) entry.name = value;
                else if (key == QLatin1String("Icon")) entry.icon = value;
                else if (key == QLatin1String("Exec")) entry.exec = cleanExec(value);
                else if (key == QLatin1String("MimeType")) entry.mimeTypes = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
                else if (key == QLatin1String("Categories")) entry.categories = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
                else if (key == QLatin1String("Hidden")) hidden = value == QLatin1String("true");
                else if (key == QLatin1String("Type")) application = value == QLatin1String("Application");
            }
            if (application && !hidden && !entry.name.isEmpty())
                entries << entry;
        }
    }
    std::sort(entries.begin(), entries.end(), [](const DesktopEntry &a, const DesktopEntry &b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    return entries;
}

// mimeapps.list files in XDG precedence order, desktop-specific ones first.
QStringList mimeappsFiles()
{
    QStringList files;
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP", QStringLiteral("Noir")).section(QLatin1Char(':'), 0, 0).toLower();
    auto add = [&](const QString &dir) {
        files << dir + QLatin1Char('/') + desktop + QStringLiteral("-mimeapps.list") << dir + QStringLiteral("/mimeapps.list");
    };
    for (const QString &dir : QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation))
        add(dir);
    for (const QString &dir : QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation))
        add(dir);
    return files;
}

QString defaultFor(const QString &type)
{
    for (const QString &path : mimeappsFiles()) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        bool inDefaults = false;
        while (!file.atEnd()) {
            const QString line = QString::fromUtf8(file.readLine()).trimmed();
            if (line.startsWith(QLatin1Char('['))) {
                inDefaults = line == QLatin1String("[Default Applications]");
                continue;
            }
            if (inDefaults && line.section(QLatin1Char('='), 0, 0).trimmed() == type) {
                const QString first = line.section(QLatin1Char('='), 1).split(QLatin1Char(';'), Qt::SkipEmptyParts).value(0).trimmed();
                if (!first.isEmpty())
                    return first;
            }
        }
    }
    return {};
}

// Sets `type=appId` in [Default Applications] of the user's Noir-specific
// noir-mimeapps.list (XDG: desktop-specific files win over mimeapps.list), so
// choices made here leave other desktops' defaults alone. Other lines are kept.
bool writeDefaults(const QStringList &types, const QString &appId)
{
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP", QStringLiteral("Noir")).section(QLatin1Char(':'), 0, 0).toLower();
    const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                       + QLatin1Char('/') + desktop + QStringLiteral("-mimeapps.list");
    QStringList lines;
    QFile in(path);
    if (in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        lines = QString::fromUtf8(in.readAll()).split(QLatin1Char('\n'));
        in.close();
    }
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
        lines.removeLast();
    int section = lines.indexOf(QStringLiteral("[Default Applications]"));
    if (section < 0) {
        if (!lines.isEmpty())
            lines << QString();
        lines << QStringLiteral("[Default Applications]");
        section = lines.size() - 1;
    }
    int end = section + 1;
    while (end < lines.size() && !lines.at(end).startsWith(QLatin1Char('[')))
        ++end;
    for (const QString &type : types) {
        bool replaced = false;
        for (int i = section + 1; i < end; ++i) {
            if (lines.at(i).section(QLatin1Char('='), 0, 0).trimmed() == type) {
                lines[i] = type + QLatin1Char('=') + appId + QLatin1Char(';');
                replaced = true;
            }
        }
        if (!replaced) {
            // After the last non-blank line of the section.
            int at = end;
            while (at > section + 1 && lines.at(at - 1).trimmed().isEmpty())
                --at;
            lines.insert(at, type + QLatin1Char('=') + appId + QLatin1Char(';'));
            ++end;
        }
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    out.write((lines.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8());
    return out.commit();
}

QVariantMap describe(const DesktopEntry &e)
{
    return { { QStringLiteral("id"), e.id }, { QStringLiteral("name"), e.name }, { QStringLiteral("icon"), e.icon } };
}

} // namespace

DefaultApps::DefaultApps(QObject *parent)
    : QObject(parent)
{
    refresh();
}

void DefaultApps::refresh()
{
    m_categories.clear();
    const QList<DesktopEntry> entries = desktopEntries();

    for (const Category &category : categoryList()) {
        QVariantList candidates;
        for (const DesktopEntry &e : entries) {
            if (e.mimeTypes.contains(category.types.first()))
                candidates << describe(e);
        }
        const QString current = defaultFor(category.types.first());
        if (!current.isEmpty() && std::none_of(candidates.begin(), candidates.end(), [&](const QVariant &c) {
                return c.toMap().value(QStringLiteral("id")).toString() == current; }))
            candidates.prepend(QVariantMap { { QStringLiteral("id"), current }, { QStringLiteral("name"), current } });
        m_categories << QVariantMap {
            { QStringLiteral("id"), QLatin1String(category.id) },
            { QStringLiteral("title"), QLatin1String(category.title) },
            { QStringLiteral("icon"), QLatin1String(category.icon) },
            { QStringLiteral("current"), current },
            { QStringLiteral("candidates"), candidates },
        };
    }

    // Terminal: the command NoirConfig holds; candidates are terminal emulators.
    const QString terminal = NoirConfig::instance()->terminal();
    QVariantList terminals { QVariantMap { { QStringLiteral("id"), QStringLiteral("x-terminal-emulator") },
                                           { QStringLiteral("name"), QStringLiteral("System default (x-terminal-emulator)") } } };
    for (const DesktopEntry &e : entries) {
        if (!e.categories.contains(QStringLiteral("TerminalEmulator")) || e.exec.isEmpty())
            continue;
        const QString program = QProcess::splitCommand(e.exec).value(0);
        terminals << QVariantMap { { QStringLiteral("id"), program }, { QStringLiteral("name"), e.name }, { QStringLiteral("icon"), e.icon } };
    }
    const QString currentProgram = QProcess::splitCommand(terminal).value(0);
    if (std::none_of(terminals.begin(), terminals.end(), [&](const QVariant &c) {
            return c.toMap().value(QStringLiteral("id")).toString() == currentProgram; }))
        terminals << QVariantMap { { QStringLiteral("id"), terminal }, { QStringLiteral("name"), terminal } };
    m_categories << QVariantMap {
        { QStringLiteral("id"), QStringLiteral("terminal") },
        { QStringLiteral("title"), QStringLiteral("Terminal") },
        { QStringLiteral("icon"), QStringLiteral("terminal") },
        { QStringLiteral("current"), currentProgram },
        { QStringLiteral("candidates"), terminals },
    };
    emit changed();
}

void DefaultApps::setDefault(const QString &category, const QString &appId)
{
    if (category == QLatin1String("terminal")) {
        NoirConfig::instance()->setTerminal(appId);
        refresh();
        return;
    }
    for (const Category &c : categoryList()) {
        if (category != QLatin1String(c.id))
            continue;
        if (!writeDefaults(c.types, appId))
            emit errorOccurred(QStringLiteral("Could not write the default applications list."));
    }
    refresh();
}
