#include "Platform.h"

#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace Platform {

// --- X11 ---------------------------------------------------------------------

xcb_connection_t *xConnection()
{
    if (auto *gui = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        if (auto *x11 = gui->nativeInterface<QNativeInterface::QX11Application>())
            return x11->connection();
    }
    static xcb_connection_t *own = [] {
        xcb_connection_t *c = xcb_connect(nullptr, nullptr);
        if (xcb_connection_has_error(c)) {
            xcb_disconnect(c);
            return static_cast<xcb_connection_t *>(nullptr);
        }
        return c;
    }();
    return own;
}

xcb_screen_t *xScreen()
{
    xcb_connection_t *c = xConnection();
    return c ? xcb_setup_roots_iterator(xcb_get_setup(c)).data : nullptr;
}

xcb_atom_t xAtom(const char *name, bool onlyIfExists)
{
    xcb_connection_t *c = xConnection();
    if (!c)
        return XCB_ATOM_NONE;
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(
        c, xcb_intern_atom(c, onlyIfExists, uint16_t(strlen(name)), name), nullptr);
    const xcb_atom_t atom = reply ? reply->atom : XCB_ATOM_NONE;
    free(reply);
    return atom;
}

bool hasExtension(xcb_extension_t *extension)
{
    xcb_connection_t *c = xConnection();
    if (!c)
        return false;
    const xcb_query_extension_reply_t *data = xcb_get_extension_data(c, extension);
    return data && data->present;
}

QString xAtomName(xcb_atom_t atom)
{
    xcb_connection_t *c = xConnection();
    if (!c || atom == XCB_ATOM_NONE)
        return {};
    xcb_get_atom_name_reply_t *reply = xcb_get_atom_name_reply(c, xcb_get_atom_name(c, atom), nullptr);
    if (!reply)
        return {};
    const QString name = QString::fromLatin1(xcb_get_atom_name_name(reply), xcb_get_atom_name_name_length(reply));
    free(reply);
    return name;
}

// --- D-Bus -------------------------------------------------------------------

bool hasService(const QDBusConnection &bus, const QString &service)
{
    if (!bus.isConnected())
        return false;
    QDBusConnectionInterface *iface = bus.interface();
    if (iface->isServiceRegistered(service))
        return true;
    const QDBusMessage reply = call(bus, QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
                                    QStringLiteral("org.freedesktop.DBus"), QStringLiteral("ListActivatableNames"));
    return reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()
        && reply.arguments().first().toStringList().contains(service);
}

QDBusMessage call(const QDBusConnection &bus, const QString &service, const QString &path,
                  const QString &interface, const QString &method, const QVariantList &args, int timeoutMs)
{
    QDBusMessage message = QDBusMessage::createMethodCall(service, path, interface, method);
    message.setArguments(args);
    return bus.call(message, QDBus::Block, timeoutMs);
}

QVariantMap getAll(const QDBusConnection &bus, const QString &service, const QString &path, const QString &interface)
{
    const QDBusMessage reply = call(bus, service, path, QStringLiteral("org.freedesktop.DBus.Properties"),
                                    QStringLiteral("GetAll"), { interface });
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return {};
    return qdbus_cast<QVariantMap>(reply.arguments().first());
}

QVariant property(const QDBusConnection &bus, const QString &service, const QString &path,
                  const QString &interface, const QString &name)
{
    const QDBusMessage reply = call(bus, service, path, QStringLiteral("org.freedesktop.DBus.Properties"),
                                    QStringLiteral("Get"), { interface, name });
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return {};
    return qdbus_cast<QDBusVariant>(reply.arguments().first()).variant();
}

void callAsync(QObject *context, const QDBusConnection &bus, const QString &service, const QString &path,
               const QString &interface, const QString &method, const QVariantList &args, Reply done)
{
    QDBusMessage message = QDBusMessage::createMethodCall(service, path, interface, method);
    message.setArguments(args);
    message.setInteractiveAuthorizationAllowed(true);
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message, 120000), context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context, [watcher, done] {
        const QDBusMessage reply = watcher->reply();
        watcher->deleteLater();
        if (done)
            done(reply.type() == QDBusMessage::ErrorMessage ? describeError(reply) : QString(), reply);
    });
}

void setPropertyAsync(QObject *context, const QDBusConnection &bus, const QString &service, const QString &path,
                      const QString &interface, const QString &name, const QVariant &value, Reply done)
{
    callAsync(context, bus, service, path, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Set"),
              { interface, name, QVariant::fromValue(QDBusVariant(value)) }, std::move(done));
}

QString describeError(const QDBusMessage &error)
{
    const QString name = error.errorName();
    QString text = error.errorMessage().isEmpty() ? name : error.errorMessage();
    if (name == QLatin1String("org.freedesktop.DBus.Error.InteractiveAuthorizationRequired")
        || text.contains(QLatin1String("Interactive authentication required"), Qt::CaseInsensitive)) {
        text = QStringLiteral("Administrator authorisation is required, and no polkit authentication agent is running in this session.");
    } else if (name == QLatin1String("org.freedesktop.DBus.Error.AccessDenied")
               || name.endsWith(QLatin1String(".NotAuthorized")) || name.endsWith(QLatin1String(".PermissionDenied"))) {
        text = QStringLiteral("Not authorised: ") + text;
    } else if (name == QLatin1String("org.freedesktop.DBus.Error.ServiceUnknown")) {
        text = QStringLiteral("The system service is not available.");
    }
    return text;
}

// --- session.conf ------------------------------------------------------------

QString sessionConfPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/noir/session.conf");
}

namespace {

QRegularExpression variableLine(const QString &name)
{
    return QRegularExpression(QStringLiteral("^\\s*(export\\s+)?%1=(.*)$").arg(QRegularExpression::escape(name)));
}

QString unquote(QString value)
{
    value = value.trimmed();
    if (value.size() >= 2 && (value.startsWith(QLatin1Char('"')) || value.startsWith(QLatin1Char('\'')))
        && value.endsWith(value.at(0)))
        value = value.mid(1, value.size() - 2);
    return value;
}

QString shellQuote(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QLatin1String("\\\\"))
           .replace(QLatin1Char('"'), QLatin1String("\\\""))
           .replace(QLatin1Char('$'), QLatin1String("\\$"))
           .replace(QLatin1Char('`'), QLatin1String("\\`"));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

} // namespace

QString sessionVariable(const QString &name)
{
    QFile file(sessionConfPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QRegularExpression re = variableLine(name);
    QString value;
    for (const QString &line : QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'))) {
        const QRegularExpressionMatch m = re.match(line);
        if (m.hasMatch())
            value = unquote(m.captured(2));   // the last assignment wins, as in the shell
    }
    return value;
}

bool setSessionVariable(const QString &name, const QString &value, bool exported)
{
    const QString path = sessionConfPath();
    QStringList lines;
    QFile in(path);
    if (in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        lines = QString::fromUtf8(in.readAll()).split(QLatin1Char('\n'));
        in.close();
        if (!lines.isEmpty() && lines.last().isEmpty())
            lines.removeLast();
    } else {
        lines << QStringLiteral("# Noir session settings, read by noir-session at login.")
              << QStringLiteral("# The Control Center maintains the variables it knows about; other lines are kept.");
    }
    const QString assignment = (exported ? QStringLiteral("export ") : QString()) + name + QLatin1Char('=') + shellQuote(value);
    const QRegularExpression re = variableLine(name);
    bool placed = false;
    for (int i = 0; i < lines.size(); ++i) {
        if (!re.match(lines.at(i)).hasMatch())
            continue;
        if (placed || value.isEmpty()) {
            lines.removeAt(i--);
        } else {
            lines[i] = assignment;
            placed = true;
        }
    }
    if (!placed && !value.isEmpty())
        lines << assignment;

    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    out.write((lines.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8());
    return out.commit();
}

// --- Processes ---------------------------------------------------------------

QString run(const QString &program, const QStringList &args, QString *error, int timeoutMs)
{
    QProcess process;
    process.start(program, args);
    if (!process.waitForStarted(timeoutMs)) {
        if (error)
            *error = program + QStringLiteral(" is not installed");
        return {};
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(500);
        if (error)
            *error = program + QStringLiteral(" did not answer");
        return {};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            const QString err = QString::fromUtf8(process.readAllStandardError()).trimmed();
            *error = err.isEmpty() ? program + QStringLiteral(" failed") : err;
        }
        return {};
    }
    const QString out = QString::fromUtf8(process.readAllStandardOutput());
    return out.isNull() ? QStringLiteral("") : out;
}

} // namespace Platform
