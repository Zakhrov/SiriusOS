#include "ControlCenterService.h"
#include "DisplayConfig.h"
#include "InputConfig.h"
#include "PowerConfig.h"

#include "NoirConfig.h"
#include "NoirVulkan.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QtQml/QQmlExtensionPlugin>

#include <cstring>

Q_IMPORT_QML_PLUGIN(Noir_UiPlugin)

namespace {

// --apply: put the saved hardware settings (displays, keyboard, pointers,
// screen blanking) back in place. noir-session runs this at login, before
// the shell starts. No window, so no GUI application is needed.
int applySavedSettings(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SiriusOS"));
    NoirConfig::instance();
    const QString error = DisplayConfig().applySaved();
    if (!error.isEmpty())
        qWarning("NoirControlCenter: displays: %s", qPrintable(error));
    InputConfig::applySaved();
    PowerConfig::applySaved();
    return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--apply") == 0)
            return applySavedSettings(argc, argv);
    }

    // X11 only, by design.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "xcb");

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("NoirControlCenter"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Control Center"));
    QGuiApplication::setOrganizationName(QStringLiteral("SiriusOS"));
    QGuiApplication::setApplicationVersion(QStringLiteral(NOIR_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral("org.siriusos.NoirControlCenter"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/Noir/Ui/icons/noir-settings.svg")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("SiriusOS Noir Control Center: system and hardware settings"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption pageOption(QStringLiteral("page"),
        QStringLiteral("Open on <page>: about, appearance, display, sound, network, bluetooth, power, input, "
                       "datetime, notifications, apps or graphics."),
        QStringLiteral("page"));
    QCommandLineOption applyOption(QStringLiteral("apply"),
        QStringLiteral("Apply the saved display, input and power settings, then exit (used at login)."));
    parser.addOption(pageOption);
    parser.addOption(applyOption);
    parser.process(app);

    ControlCenterService *service = ControlCenterService::instance();
    service->setInitialPage(parser.value(pageOption));
    if (!service->registerOnBus()) {
        if (ControlCenterService::forwardToRunningInstance(parser.value(pageOption)))
            return EXIT_SUCCESS;
        qWarning("NoirControlCenter: another instance holds the bus name but did not answer; starting anyway");
    }

    // An ordinary application window: opaque, so no compositor is needed.
    bool translucent = false;
    if (!Noir::setUpVulkan(&app, "NoirControlCenter", false, &translucent))
        return EXIT_FAILURE;
    NoirConfig::instance();

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.addImportPath(QStringLiteral("qrc:/"));
    engine.load(QUrl(QStringLiteral("qrc:/Noir/ControlCenter/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return EXIT_FAILURE;
    return app.exec();
}
