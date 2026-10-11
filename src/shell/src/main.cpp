#include "AppModel.h"
#include "NotificationServer.h"
#include "TaskModel.h"
#include "X11Integration.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSize>

int main(int argc, char *argv[])
{
    // The Noir stack is Vulkan only; Qt Quick renders through its Vulkan RHI
    // unless the developer overrides it (e.g. QSG_RHI_BACKEND=opengl).
    if (qEnvironmentVariableIsEmpty("QSG_RHI_BACKEND"))
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    // X11 only, by design.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "xcb");

    // Popups and notifications use translucent surfaces composited by NoirWM.
    QQuickWindow::setDefaultAlphaBuffer(true);

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("NoirShell"));
    QGuiApplication::setOrganizationName(QStringLiteral("SiriusOS"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1"));
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
    parser.addOption(windowedOption);
    parser.addOption(demoOption);
    parser.process(app);

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
    NotificationServer::instance();

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("windowicon"), new WindowIconProvider);
    engine.addImageProvider(QStringLiteral("appicon"), new AppIconProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("shellWindowed"), windowedSize.isValid());
    engine.rootContext()->setContextProperty(QStringLiteral("shellWindowedSize"), windowedSize);
    engine.rootContext()->setContextProperty(QStringLiteral("shellDemo"), parser.isSet(demoOption));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.addImportPath(QStringLiteral("qrc:/"));
    engine.load(QUrl(QStringLiteral("qrc:/Noir/Shell/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return EXIT_FAILURE;

    return app.exec();
}
