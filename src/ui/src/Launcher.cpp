#include "Launcher.h"

#include <QProcess>

bool Launcher::run(const QString &command)
{
    QStringList args = QProcess::splitCommand(command);
    if (args.isEmpty())
        return false;
    const QString program = args.takeFirst();
    return QProcess::startDetached(program, args);
}

bool Launcher::openControlCenter(const QString &page)
{
    QStringList args;
    if (!page.isEmpty())
        args << QStringLiteral("--page") << page;
    return QProcess::startDetached(QStringLiteral("NoirControlCenter"), args);
}
