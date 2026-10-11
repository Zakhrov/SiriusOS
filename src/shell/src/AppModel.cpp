#include "AppModel.h"

#include <QDirIterator>
#include <QIcon>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>

namespace {

// Strip desktop-entry field codes (%f, %U, ...) and unescape %%.
QString cleanExec(const QString &exec)
{
    QString out;
    out.reserve(exec.size());
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

QString primaryCategory(const QString &categories)
{
    static const QStringList known = {
        QStringLiteral("System"), QStringLiteral("Development"), QStringLiteral("Graphics"),
        QStringLiteral("Network"), QStringLiteral("AudioVideo"), QStringLiteral("Office"),
        QStringLiteral("Game"), QStringLiteral("Settings"), QStringLiteral("Utility"),
    };
    const QStringList cats = categories.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &k : known) {
        if (cats.contains(k))
            return k;
    }
    return QStringLiteral("Other");
}

} // namespace

AppModel::AppModel(QObject *parent)
    : QAbstractListModel(parent)
{
    reload();
}

int AppModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_apps.size();
}

QVariant AppModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_apps.size())
        return {};
    const App &app = m_apps.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole: return app.name;
    case CommentRole: return app.comment;
    case IconRole: return app.icon;
    case ExecRole: return app.exec;
    case CategoryRole: return app.category;
    case TerminalRole: return app.terminal;
    }
    return {};
}

QHash<int, QByteArray> AppModel::roleNames() const
{
    return {
        { NameRole, "name" },
        { CommentRole, "comment" },
        { IconRole, "iconName" },
        { ExecRole, "exec" },
        { CategoryRole, "category" },
        { TerminalRole, "terminal" },
    };
}

bool AppModel::parseDesktopFile(const QString &path, App &app)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    const QString lang = QLocale().name();            // e.g. en_US
    const QString shortLang = lang.section(QLatin1Char('_'), 0, 0);
    QString localizedName, localizedComment, categories, type;
    bool inEntry = false, hidden = false;

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            inEntry = line == QLatin1String("[Desktop Entry]");
            continue;
        }
        if (!inEntry || line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq < 0)
            continue;
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();

        if (key == QLatin1String("Name")) app.name = value;
        else if (key == QLatin1String("Name[%1]").arg(lang) || (key == QLatin1String("Name[%1]").arg(shortLang) && localizedName.isEmpty())) localizedName = value;
        else if (key == QLatin1String("Comment")) app.comment = value;
        else if (key == QLatin1String("Comment[%1]").arg(lang) || (key == QLatin1String("Comment[%1]").arg(shortLang) && localizedComment.isEmpty())) localizedComment = value;
        else if (key == QLatin1String("Icon")) app.icon = value;
        else if (key == QLatin1String("Exec")) app.exec = cleanExec(value);
        else if (key == QLatin1String("Categories")) categories = value;
        else if (key == QLatin1String("Type")) type = value;
        else if (key == QLatin1String("Terminal")) app.terminal = value == QLatin1String("true");
        else if (key == QLatin1String("NoDisplay") || key == QLatin1String("Hidden")) hidden = hidden || value == QLatin1String("true");
        else if (key == QLatin1String("OnlyShowIn")) hidden = hidden || !value.split(QLatin1Char(';')).contains(QLatin1String("Noir"));
        else if (key == QLatin1String("NotShowIn")) hidden = hidden || value.split(QLatin1Char(';')).contains(QLatin1String("Noir"));
    }

    if (!localizedName.isEmpty()) app.name = localizedName;
    if (!localizedComment.isEmpty()) app.comment = localizedComment;
    app.category = primaryCategory(categories);
    return !hidden && type == QLatin1String("Application") && !app.name.isEmpty() && !app.exec.isEmpty();
}

void AppModel::reload()
{
    beginResetModel();
    m_apps.clear();

    // Earlier locations (user dirs) shadow later ones with the same desktop id.
    QSet<QString> seen;
    const QStringList dirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    for (const QString &dir : dirs) {
        QDirIterator it(dir, { QStringLiteral("*.desktop") }, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            QString id = path.mid(dir.size() + 1);
            id.replace(QLatin1Char('/'), QLatin1Char('-'));
            if (seen.contains(id))
                continue;
            seen.insert(id);
            App app;
            app.id = id;
            if (parseDesktopFile(path, app))
                m_apps.append(app);
        }
    }

    std::sort(m_apps.begin(), m_apps.end(), [](const App &a, const App &b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    endResetModel();
    emit countChanged();
}

bool AppModel::launch(int row)
{
    if (row < 0 || row >= m_apps.size())
        return false;
    const App &app = m_apps.at(row);
    QString command = app.exec;
    if (app.terminal)
        command = QStringLiteral("x-terminal-emulator -e ") + command;
    const bool ok = run(command);
    if (ok)
        emit launched(app.name);
    return ok;
}

bool AppModel::run(const QString &command)
{
    QStringList args = QProcess::splitCommand(command);
    if (args.isEmpty())
        return false;
    const QString program = args.takeFirst();
    return QProcess::startDetached(program, args);
}

AppFilterModel::AppFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    connect(this, &QAbstractItemModel::rowsInserted, this, &AppFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &AppFilterModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &AppFilterModel::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &AppFilterModel::countChanged);
}

void AppFilterModel::setQuery(const QString &query)
{
    if (m_query == query)
        return;
    m_query = query;
    invalidateFilter();
    emit queryChanged();
    emit countChanged();
}

int AppFilterModel::sourceRow(int row) const
{
    return mapToSource(index(row, 0)).row();
}

bool AppFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_query.isEmpty())
        return true;
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    return idx.data(AppModel::NameRole).toString().contains(m_query, Qt::CaseInsensitive)
        || idx.data(AppModel::CommentRole).toString().contains(m_query, Qt::CaseInsensitive)
        || idx.data(AppModel::ExecRole).toString().contains(m_query, Qt::CaseInsensitive);
}

AppIconProvider::AppIconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

QPixmap AppIconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QSize target = requestedSize.isValid() ? requestedSize : QSize(48, 48);
    const QString name = QUrl::fromPercentEncoding(id.toUtf8());
    QIcon icon = QFileInfo(name).isAbsolute() ? QIcon(name) : QIcon::fromTheme(name);
    QPixmap pixmap = icon.pixmap(target);
    if (pixmap.isNull()) {
        pixmap = QPixmap(1, 1);
        pixmap.fill(Qt::transparent);
    }
    if (size)
        *size = pixmap.size();
    return pixmap;
}
