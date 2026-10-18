#pragma once

#include <QDBusConnection>
#include <QDBusMessage>
#include <QString>
#include <QVariant>

#include <functional>
#include <xcb/xcb.h>

// Small helpers the Control Center backends share: the X connection, D-Bus
// calls that never block for long, and ~/.config/noir/session.conf.
namespace Platform {

// --- X11 ---------------------------------------------------------------------
// The application's X connection when it has a GUI, otherwise (--apply) one
// of our own. nullptr without an X server.
xcb_connection_t *xConnection();
xcb_screen_t *xScreen();
xcb_atom_t xAtom(const char *name, bool onlyIfExists = false);
// Whether the X server has `extension`. Any request to a missing extension
// closes the whole connection, so check before the first one.
bool hasExtension(xcb_extension_t *extension);
QString xAtomName(xcb_atom_t atom);

// --- D-Bus -------------------------------------------------------------------
// Calls never use QDBusInterface, whose constructor introspects the service
// synchronously; they go out as plain messages with a timeout instead.
constexpr int CallTimeoutMs = 2500;

// Whether `service` is running or can be started on demand.
bool hasService(const QDBusConnection &bus, const QString &service);

QDBusMessage call(const QDBusConnection &bus, const QString &service, const QString &path,
                  const QString &interface, const QString &method, const QVariantList &args = {},
                  int timeoutMs = CallTimeoutMs);
QVariantMap getAll(const QDBusConnection &bus, const QString &service, const QString &path,
                   const QString &interface);
QVariant property(const QDBusConnection &bus, const QString &service, const QString &path,
                  const QString &interface, const QString &name);

// Asynchronous call; `done` receives the error message ("" on success) and
// the reply. Long timeout: the call may wait for a polkit prompt.
using Reply = std::function<void(const QString &error, const QDBusMessage &reply)>;
void callAsync(QObject *context, const QDBusConnection &bus, const QString &service, const QString &path,
               const QString &interface, const QString &method, const QVariantList &args, Reply done);
void setPropertyAsync(QObject *context, const QDBusConnection &bus, const QString &service,
                      const QString &path, const QString &interface, const QString &name,
                      const QVariant &value, Reply done);

// A human-readable form of a D-Bus error, with a hint for the usual polkit
// failure in a session that has no authentication agent.
QString describeError(const QDBusMessage &error);

// --- session.conf ------------------------------------------------------------
// ~/.config/noir/session.conf is plain shell read by noir-session at login.
// These read and set single variables in it and keep every other line.
QString sessionConfPath();
QString sessionVariable(const QString &name);
// An empty value removes the variable. `exported` writes `export NAME=...`.
bool setSessionVariable(const QString &name, const QString &value, bool exported);

// --- Processes ---------------------------------------------------------------
// Runs a program and waits up to `timeoutMs`. Returns its standard output, or
// a null string when it could not run or failed (`error` then says why).
QString run(const QString &program, const QStringList &args, QString *error = nullptr, int timeoutMs = 3000);

} // namespace Platform
