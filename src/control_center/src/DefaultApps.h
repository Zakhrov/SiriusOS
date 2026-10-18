#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// Default Applications page: the web browser, file manager, e-mail client,
// text editor, image viewer, video and music players (XDG mimeapps.list) and
// the terminal (NoirConfig, used by the shell's Terminal launcher and for
// applications that run in a terminal).
class DefaultApps : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    // {id, title, icon, current, candidates: [{id, name, icon}]}
    Q_PROPERTY(QVariantList categories READ categories NOTIFY changed)

public:
    explicit DefaultApps(QObject *parent = nullptr);

    QVariantList categories() const { return m_categories; }

    Q_INVOKABLE void refresh();
    // `appId` is a desktop file id, or for the terminal its command.
    Q_INVOKABLE void setDefault(const QString &category, const QString &appId);

signals:
    void changed();
    void errorOccurred(const QString &message);

private:
    QVariantList m_categories;
};
