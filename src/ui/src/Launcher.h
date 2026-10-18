#pragma once

#include <QObject>
#include <QQmlEngine>

// Starts programs on behalf of QML: a raw command line, or the Control
// Center on one of its pages.
class Launcher : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    // Splits `command` like a shell would (quotes, no expansion) and starts
    // it detached. Returns false when it could not be started.
    Q_INVOKABLE static bool run(const QString &command);
    // `page` is a Control Center page id ("display", "sound", ...) or empty.
    Q_INVOKABLE static bool openControlCenter(const QString &page = {});
};
