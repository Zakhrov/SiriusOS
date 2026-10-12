#include "DebugInterface.h"

#include <QDBusConnection>
#include <QGuiApplication>
#include <QImage>
#include <QJSValue>
#include <QJsonDocument>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTextStream>

namespace {

QList<QQuickWindow *> matchingWindows(const QString &pattern)
{
    QList<QQuickWindow *> result;
    bool isId = false;
    const qulonglong id = pattern.toULongLong(&isId, 0);
    const auto windows = QGuiApplication::allWindows();
    for (QWindow *window : windows) {
        auto *quick = qobject_cast<QQuickWindow *>(window);
        if (!quick)
            continue;
        if (pattern.isEmpty() || (isId && quick->winId() == id)
            || quick->title().contains(pattern, Qt::CaseInsensitive))
            result.append(quick);
    }
    return result;
}

// "TaskButton_QMLTYPE_12" -> "TaskButton", "QQuickRectangle" stays as is.
QString typeName(const QObject *object)
{
    static const QRegularExpression suffix(QStringLiteral("_QML(TYPE)?_\\d+$"));
    return QString::fromLatin1(object->metaObject()->className()).remove(suffix);
}

QString quoted(QString text)
{
    if (text.size() > 60)
        text = text.left(57) + QStringLiteral("...");
    text.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return QLatin1Char('"') + text + QLatin1Char('"');
}

void dumpItem(QQuickItem *item, const QPoint &origin, int depth, bool visibleOnly, QTextStream &out)
{
    if (visibleOnly && !item->isVisible())
        return;

    out << QString(depth * 2, QLatin1Char(' ')) << typeName(item);
    if (QQmlContext *context = qmlContext(item)) {
        const QString id = context->nameForObject(item);
        if (!id.isEmpty())
            out << '#' << id;
    }
    if (!item->objectName().isEmpty())
        out << '(' << item->objectName() << ')';

    const QRect rect = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()))
                           .toRect().translated(origin);
    out << ' ' << rect.x() << ',' << rect.y() << ' ' << rect.width() << 'x' << rect.height();

    for (const char *name : { "text", "title", "summary", "toolTip", "source", "iconSource" }) {
        const QVariant value = item->property(name);
        if (value.isValid() && !value.toString().isEmpty())
            out << ' ' << name << '=' << quoted(value.toString());
    }
    if (!item->isVisible())
        out << " [hidden]";
    if (!item->isEnabled())
        out << " [disabled]";
    if (item->hasActiveFocus())
        out << " [focus]";
    if (item->opacity() < 1.0)
        out << " [opacity " << item->opacity() << ']';
    out << '\n';

    const auto children = item->childItems();
    for (QQuickItem *child : children)
        dumpItem(child, origin, depth + 1, visibleOnly, out);
}

void describeWindow(QQuickWindow *window, QTextStream &out)
{
    out << "0x" << Qt::hex << window->winId() << Qt::dec << ' '
        << window->x() << ',' << window->y() << ' ' << window->width() << 'x' << window->height()
        << (window->isVisible() ? " visible" : " hidden")
        << (window->isActive() ? " active" : "")
        << ' ' << quoted(window->title()) << '\n';
}

} // namespace

DebugInterface::DebugInterface(QQmlApplicationEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
}

bool DebugInterface::registerOnBus()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    return bus.isConnected()
        && bus.registerObject(QStringLiteral("/Debug"), this, QDBusConnection::ExportAllSlots)
        && bus.registerService(QStringLiteral("org.siriusos.NoirShell"));
}

QString DebugInterface::Windows()
{
    QString text;
    QTextStream out(&text);
    const auto windows = matchingWindows(QString());
    for (QQuickWindow *window : windows)
        describeWindow(window, out);
    return text;
}

QString DebugInterface::ItemTree(const QString &window, bool visibleOnly)
{
    QString text;
    QTextStream out(&text);
    const auto windows = matchingWindows(window);
    for (QQuickWindow *quick : windows) {
        if (visibleOnly && !quick->isVisible())
            continue;
        describeWindow(quick, out);
        dumpItem(quick->contentItem(), quick->position(), 1, visibleOnly, out);
    }
    return text;
}

QString DebugInterface::Grab(const QString &window, const QString &path)
{
    const auto windows = matchingWindows(window);
    if (windows.isEmpty())
        return QStringLiteral("no window matches \"%1\"").arg(window);
    QQuickWindow *quick = windows.first();
    if (!quick->isVisible())
        return QStringLiteral("\"%1\" is not visible").arg(quick->title());
    const QImage image = quick->grabWindow();
    if (image.isNull())
        return QStringLiteral("could not render \"%1\"").arg(quick->title());
    if (!image.save(path))
        return QStringLiteral("could not write %1").arg(path);
    return QString();
}

QString DebugInterface::Eval(const QString &expression)
{
    QObject *root = m_engine->rootObjects().value(0);
    if (!root)
        return QStringLiteral("error: no root object");

    QQmlExpression script(qmlContext(root), root, expression);
    QVariant value = script.evaluate();
    if (script.hasError())
        return QStringLiteral("error: ") + script.error().toString();

    if (value.userType() == qMetaTypeId<QJSValue>())
        value = value.value<QJSValue>().toVariant();
    if (!value.isValid())
        return QStringLiteral("undefined");
    if (value.typeId() == QMetaType::QVariantMap || value.typeId() == QMetaType::QVariantList
        || value.typeId() == QMetaType::QStringList)
        return QString::fromUtf8(QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact));
    if (auto *object = value.value<QObject *>())
        return typeName(object);
    return value.toString();
}
