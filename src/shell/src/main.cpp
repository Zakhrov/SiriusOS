#include "AppModel.h"
#include "DebugInterface.h"
#include "NotificationServer.h"
#include "TaskModel.h"
#include "X11Integration.h"

#include "NoirConfig.h"
#include "NoirVulkan.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSize>
#include <QtQml/QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(Noir_UiPlugin)

int main(int argc, char *argv[])
{
    // X11 only, by design.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "xcb");

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("NoirShell"));
    QGuiApplication::setOrganizationName(QStringLiteral("SiriusOS"));
    QGuiApplication::setApplicationVersion(QStringLiteral(NOIR_VERSION));
    QGuiApplication::setQuitOnLastWindowClosed(true);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("SiriusOS Noir desktop shell"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption windowedOption(QStringLiteral("windowed"),
        QStringLiteral("Run the whole shell inside a single window of <size> (e.g. 1280x720) for development."),
        QStringLiteral("size"));
    QCommandLineOption demoOption(QStringLiteral("demo"),
        QStringLiteral("Populate the taskbar and notifications with sample data."));
    QCommandLineOption debugOption(QStringLiteral("debug-interface"),
        QStringLiteral("Export org.siriusos.NoirShell.Debug on the session bus (item tree, grabs, QML eval)."));
    parser.addOption(windowedOption);
    parser.addOption(demoOption);
    parser.addOption(debugOption);
    parser.process(app);

    // Popups and notifications are translucent surfaces, which only a
    // compositing manager can blend. Without one an ARGB window gains nothing
    // (its alpha is ignored), so the surfaces stay opaque.
    bool composited = false;
    if (!Noir::setUpVulkan(&app, "NoirShell", Noir::compositingManagerRunning(), &composited))
        return EXIT_FAILURE;
    // Surfaces opt in to an ARGB visual one by one, by asking for a
    // transparent colour (see shellComposited in SessionShell.qml). The
    // desktop never does: it is the opaque bottom layer.
    QQuickWindow::setDefaultAlphaBuffer(false);

    QSize windowedSize;
    if (parser.isSet(windowedOption)) {
        const QStringList dims = parser.value(windowedOption).split(QLatin1Char('x'));
        windowedSize = QSize(dims.value(0).toInt(), dims.value(1).toInt());
        if (windowedSize.isEmpty())
            windowedSize = QSize(1280, 720);
    }

    // Instantiate the service singletons eagerly so the notification daemon is
    // on the bus before any QML has loaded.
    X11Integration::instance();
    NoirConfig::instance();
    NotificationServer::instance();

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("windowicon"), new WindowIconProvider);
    engine.addImageProvider(QStringLiteral("appicon"), new AppIconProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("shellWindowed"), windowedSize.isValid());
    engine.rootContext()->setContextProperty(QStringLiteral("shellWindowedSize"), windowedSize);
    engine.rootContext()->setContextProperty(QStringLiteral("shellDemo"), parser.isSet(demoOption));
    engine.rootContext()->setContextProperty(QStringLiteral("shellComposited"), composited);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.addImportPath(QStringLiteral("qrc:/"));
    engine.load(QUrl(QStringLiteral("qrc:/Noir/Shell/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return EXIT_FAILURE;

    if (parser.isSet(debugOption)) {
        auto *debug = new DebugInterface(&engine, &app);
        if (!debug->registerOnBus())
            qWarning("NoirShell: could not export the debug interface on the session bus");
    }

    return app.exec();
}
