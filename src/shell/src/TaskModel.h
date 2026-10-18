#pragma once

#include <QAbstractListModel>
#include <QAbstractNativeEventFilter>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QTimer>

#include <xcb/xcb.h>

// Live list of top-level application windows, read from the window manager
// via EWMH (_NET_CLIENT_LIST / _NET_ACTIVE_WINDOW). Drives the taskbar.
class TaskModel : public QAbstractListModel, public QAbstractNativeEventFilter
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool demo READ isDemo WRITE setDemo NOTIFY demoChanged)

public:
    enum Roles {
        WindowIdRole = Qt::UserRole + 1,
        TitleRole,
        ActiveRole,
        MinimizedRole,
        AttentionRole,
        IconSourceRole,
    };

    explicit TaskModel(QObject *parent = nullptr);
    ~TaskModel() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool isDemo() const { return m_demo; }
    void setDemo(bool demo);

    Q_INVOKABLE void activate(int row);
    Q_INVOKABLE void minimize(int row);
    Q_INVOKABLE void close(int row);
    // Taskbar click semantics: minimize the active window, otherwise raise it.
    Q_INVOKABLE void toggle(int row);
    Q_INVOKABLE void setIconGeometry(int row, QQuickItem *item);

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

signals:
    void countChanged();
    void demoChanged();

private:
    struct Task {
        xcb_window_t id = 0;
        QString title;
        bool minimized = false;
        bool attention = false;
        bool skip = false;
        int iconSerial = 0;
        QRect iconGeometry;
    };

    void scheduleRefresh();
    void refresh();
    void loadDemoTasks();
    void readTask(Task &task);
    int rowOf(xcb_window_t id) const;
    void sendClientMessage(xcb_window_t window, xcb_atom_t type, const quint32 (&data)[5]);

    QList<Task> m_tasks;
    xcb_window_t m_activeWindow = 0;
    bool m_demo = false;
    QTimer m_refreshTimer;
};

// Serves _NET_WM_ICON data as image://windowicon/<window id>/<serial>.
class WindowIconProvider : public QQuickImageProvider
{
public:
    WindowIconProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
