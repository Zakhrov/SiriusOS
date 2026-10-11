#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QSortFilterProxyModel>

// Installed applications, read from XDG .desktop entries. Feeds the start menu.
class AppModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        CommentRole,
        IconRole,
        ExecRole,
        CategoryRole,
        TerminalRole,
    };

    explicit AppModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void reload();
    Q_INVOKABLE bool launch(int row);
    // Launch a raw command line (used by built-in shortcuts such as "Terminal").
    Q_INVOKABLE bool run(const QString &command);

signals:
    void countChanged();
    void launched(const QString &name);

private:
    struct App {
        QString id;
        QString name;
        QString comment;
        QString icon;
        QString exec;
        QString category;
        bool terminal = false;
    };

    static bool parseDesktopFile(const QString &path, App &app);

    QList<App> m_apps;
};

// Case-insensitive search over AppModel, used by the start menu search field.
class AppFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit AppFilterModel(QObject *parent = nullptr);

    QString query() const { return m_query; }
    void setQuery(const QString &query);
    int count() const { return rowCount(); }

    Q_INVOKABLE int sourceRow(int row) const;

signals:
    void queryChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_query;
};

// Resolves third-party icon names (or absolute paths) through the icon theme
// as image://appicon/<name>. Third-party icons are shown untouched per the VDD.
class AppIconProvider : public QQuickImageProvider
{
public:
    AppIconProvider();
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;
};
