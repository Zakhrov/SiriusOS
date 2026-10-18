#include "InputConfig.h"

#include "NoirConfig.h"
#include "Platform.h"

#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>

#include <xcb/xinput.h>
// xkb.h names a struct member "explicit", a C++ keyword.
#define explicit explicit_
#include <xcb/xkb.h>
#undef explicit

#include <cstring>

namespace {

// libinput's X driver properties, by setting key.
struct PointerProperty {
    const char *key;
    const char *atom;
    bool isFloat;
};
const PointerProperty Properties[] = {
    { "accelSpeed", "libinput Accel Speed", true },
    { "flatProfile", "libinput Accel Profile Enabled", false },
    { "naturalScroll", "libinput Natural Scrolling Enabled", false },
    { "leftHanded", "libinput Left Handed Enabled", false },
    { "middleEmulation", "libinput Middle Emulation Enabled", false },
    { "tapping", "libinput Tapping Enabled", false },
    { "disableWhileTyping", "libinput Disable While Typing Enabled", false },
};

struct Device {
    xcb_input_device_id_t id;
    QString name;
    bool touchpad;
};

bool xiReady(xcb_connection_t *c)
{
    static int ready = -1;
    if (ready < 0 && !Platform::hasExtension(&xcb_input_id))
        ready = 0;
    if (ready < 0) {
        auto *version = xcb_input_xi_query_version_reply(c, xcb_input_xi_query_version(c, 2, 2), nullptr);
        ready = version && version->major_version >= 2;
        free(version);
    }
    return ready;
}

// Raw property: format (8/32), the items, and their count.
struct RawProperty {
    uint8_t format = 0;
    xcb_atom_t type = XCB_ATOM_NONE;
    QByteArray data;
    uint32_t items = 0;
};

RawProperty readProperty(xcb_connection_t *c, xcb_input_device_id_t device, xcb_atom_t property)
{
    RawProperty raw;
    if (property == XCB_ATOM_NONE)
        return raw;
    auto *reply = xcb_input_xi_get_property_reply(
        c, xcb_input_xi_get_property(c, device, 0, property, XCB_ATOM_ANY, 0, 64), nullptr);
    if (!reply)
        return raw;
    raw.format = reply->format;
    raw.type = reply->type;
    raw.items = reply->num_items;
    raw.data = QByteArray(static_cast<const char *>(xcb_input_xi_get_property_items(reply)),
                          int(reply->num_items * (reply->format / 8)));
    free(reply);
    return raw;
}

QList<Device> pointerDevices(xcb_connection_t *c)
{
    QList<Device> devices;
    if (!c || !xiReady(c))
        return devices;
    auto *reply = xcb_input_xi_query_device_reply(c, xcb_input_xi_query_device(c, XCB_INPUT_DEVICE_ALL), nullptr);
    if (!reply)
        return devices;
    const xcb_atom_t speed = Platform::xAtom("libinput Accel Speed", true);
    const xcb_atom_t tapping = Platform::xAtom("libinput Tapping Enabled", true);
    for (auto it = xcb_input_xi_query_device_infos_iterator(reply); it.rem; xcb_input_xi_device_info_next(&it)) {
        const xcb_input_xi_device_info_t *info = it.data;
        if (info->type != XCB_INPUT_DEVICE_TYPE_SLAVE_POINTER)
            continue;
        const QString name = QString::fromUtf8(xcb_input_xi_device_info_name(info), xcb_input_xi_device_info_name_length(info));
        if (name.contains(QLatin1String("XTEST")))
            continue;
        // Only libinput devices have the properties this page manages.
        if (readProperty(c, info->deviceid, speed).items == 0 && readProperty(c, info->deviceid, tapping).items == 0)
            continue;
        devices << Device { info->deviceid, name, readProperty(c, info->deviceid, tapping).items > 0 };
    }
    free(reply);
    return devices;
}

QVariant decode(const RawProperty &raw, const PointerProperty &p)
{
    if (raw.items == 0)
        return {};
    if (p.isFloat && raw.format == 32) {
        float value;
        memcpy(&value, raw.data.constData(), sizeof(float));
        return double(value);
    }
    if (raw.format != 8)
        return {};
    if (strcmp(p.key, "flatProfile") == 0)
        return raw.items >= 2 && raw.data.at(1) != 0;   // [adaptive, flat(, custom)]
    return raw.data.at(0) != 0;
}

QString configKey(bool touchpad, const QString &key)
{
    return (touchpad ? QStringLiteral("Input/Touchpad/") : QStringLiteral("Input/Mouse/")) + key;
}

} // namespace

InputConfig::InputConfig(QObject *parent)
    : QObject(parent)
{
    refresh();
}

void InputConfig::refresh()
{
    xcb_connection_t *c = Platform::xConnection();
    if (c && Platform::hasExtension(&xcb_xkb_id)) {
        if (auto *ext = xcb_xkb_use_extension_reply(c, xcb_xkb_use_extension(c, 1, 0), nullptr)) {
            free(ext);
            if (auto *ctrl = xcb_xkb_get_controls_reply(c, xcb_xkb_get_controls(c, XCB_XKB_ID_USE_CORE_KBD), nullptr)) {
                m_repeatEnabled = ctrl->enabledControls & XCB_XKB_BOOL_CTRL_REPEAT_KEYS;
                m_repeatDelay = ctrl->repeatDelay;
                m_repeatRate = ctrl->repeatInterval ? qRound(1000.0 / ctrl->repeatInterval) : 25;
                free(ctrl);
            }
        }
    }

    // setxkbmap -query reports the server's current rules.
    const QString query = Platform::run(QStringLiteral("setxkbmap"), { QStringLiteral("-query") });
    m_layouts.clear();
    m_layoutSwitch.clear();
    for (const QString &line : query.split(QLatin1Char('\n'))) {
        if (line.startsWith(QLatin1String("layout:")))
            m_layouts = line.section(QLatin1Char(':'), 1).trimmed().split(QLatin1Char(','), Qt::SkipEmptyParts);
        else if (line.startsWith(QLatin1String("options:"))) {
            for (const QString &option : line.section(QLatin1Char(':'), 1).trimmed().split(QLatin1Char(','))) {
                if (option.startsWith(QLatin1String("grp:")))
                    m_layoutSwitch = option;
            }
        }
    }

    m_pointers.clear();
    m_mouse.clear();
    m_touchpad.clear();
    for (const Device &device : pointerDevices(c)) {
        m_pointers << QVariantMap { { QStringLiteral("id"), device.id }, { QStringLiteral("name"), device.name },
                                    { QStringLiteral("touchpad"), device.touchpad } };
        QVariantMap &settings = device.touchpad ? m_touchpad : m_mouse;
        for (const PointerProperty &p : Properties) {
            if (settings.contains(QLatin1String(p.key)))
                continue;
            const QVariant value = decode(readProperty(c, device.id, Platform::xAtom(p.atom, true)), p);
            if (value.isValid())
                settings.insert(QLatin1String(p.key), value);
        }
    }
    emit changed();
}

QVariantList InputConfig::availableLayouts() const
{
    QVariantList list;
    QFile file(QStringLiteral("/usr/share/X11/xkb/rules/evdev.lst"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return list;
    bool inLayouts = false;
    static const QRegularExpression entry(QStringLiteral("^\\s+(\\S+)\\s+(.+)$"));
    for (const QString &line : QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'))) {
        if (line.startsWith(QLatin1Char('!'))) {
            inLayouts = line.trimmed() == QLatin1String("! layout");
            continue;
        }
        if (!inLayouts)
            continue;
        const QRegularExpressionMatch m = entry.match(line);
        if (m.hasMatch())
            list << QVariantMap { { QStringLiteral("code"), m.captured(1) },
                                  { QStringLiteral("text"), m.captured(2).trimmed() + QStringLiteral(" (") + m.captured(1) + QLatin1Char(')') } };
    }
    std::sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("text")).toString().localeAwareCompare(b.toMap().value(QStringLiteral("text")).toString()) < 0;
    });
    return list;
}

bool InputConfig::layoutsSupported() const
{
    return !QStandardPaths::findExecutable(QStringLiteral("setxkbmap")).isEmpty();
}

void InputConfig::setRepeat(bool enabled, int delayMs, int ratePerSecond)
{
    NoirConfig *config = NoirConfig::instance();
    config->setValue(QStringLiteral("Input/Keyboard/RepeatEnabled"), enabled);
    config->setValue(QStringLiteral("Input/Keyboard/RepeatDelay"), delayMs);
    config->setValue(QStringLiteral("Input/Keyboard/RepeatRate"), ratePerSecond);
    if (!applyRepeat(enabled, delayMs, ratePerSecond))
        emit errorOccurred(QStringLiteral("The X server did not accept the key repeat settings."));
    refresh();
}

void InputConfig::setLayouts(const QStringList &layouts, const QString &switchOption)
{
    NoirConfig *config = NoirConfig::instance();
    config->setValue(QStringLiteral("Input/Keyboard/Layouts"), layouts.join(QLatin1Char(',')));
    config->setValue(QStringLiteral("Input/Keyboard/LayoutSwitch"), switchOption);
    const QString error = applyLayouts(layouts, switchOption);
    if (!error.isEmpty())
        emit errorOccurred(error);
    refresh();
}

void InputConfig::setPointerSetting(bool touchpad, const QString &key, const QVariant &value)
{
    NoirConfig::instance()->setValue(configKey(touchpad, key), value);
    applyPointer(touchpad, key, value);
    refresh();
}

bool InputConfig::applyRepeat(bool enabled, int delayMs, int ratePerSecond)
{
    xcb_connection_t *c = Platform::xConnection();
    if (!c || !Platform::hasExtension(&xcb_xkb_id))
        return false;
    auto *ext = xcb_xkb_use_extension_reply(c, xcb_xkb_use_extension(c, 1, 0), nullptr);
    if (!ext)
        return false;
    free(ext);
    auto *ctrl = xcb_xkb_get_controls_reply(c, xcb_xkb_get_controls(c, XCB_XKB_ID_USE_CORE_KBD), nullptr);
    if (!ctrl)
        return false;
    const uint16_t delay = uint16_t(std::clamp(delayMs, 100, 2000));
    const uint16_t interval = uint16_t(std::lround(1000.0 / std::clamp(ratePerSecond, 2, 100)));
    xcb_generic_error_t *error = xcb_request_check(c, xcb_xkb_set_controls_checked(
        c, XCB_XKB_ID_USE_CORE_KBD, 0, 0, 0, 0, 0, 0, 0, 0,
        ctrl->mouseKeysDfltBtn, ctrl->groupsWrap, ctrl->accessXOption,
        XCB_XKB_BOOL_CTRL_REPEAT_KEYS, enabled ? XCB_XKB_BOOL_CTRL_REPEAT_KEYS : 0,
        XCB_XKB_BOOL_CTRL_REPEAT_KEYS | XCB_XKB_CONTROL_CONTROLS_ENABLED,
        delay, interval, ctrl->slowKeysDelay, ctrl->debounceDelay,
        ctrl->mouseKeysDelay, ctrl->mouseKeysInterval, ctrl->mouseKeysTimeToMax, ctrl->mouseKeysMaxSpeed,
        ctrl->mouseKeysCurve, ctrl->accessXTimeout, ctrl->accessXTimeoutMask, ctrl->accessXTimeoutValues,
        ctrl->accessXTimeoutOptionsMask, ctrl->accessXTimeoutOptionsValues, ctrl->perKeyRepeat));
    free(ctrl);
    const bool ok = !error;
    free(error);
    return ok;
}

QString InputConfig::applyLayouts(const QStringList &layouts, const QString &switchOption)
{
    if (layouts.isEmpty())
        return {};
    // "-option" with an empty value clears the old options before the new one.
    QStringList args { QStringLiteral("-layout"), layouts.join(QLatin1Char(',')), QStringLiteral("-option"), QString() };
    if (!switchOption.isEmpty() && layouts.size() > 1)
        args << QStringLiteral("-option") << switchOption;
    QString error;
    if (Platform::run(QStringLiteral("setxkbmap"), args, &error).isNull())
        return QStringLiteral("Could not set the keyboard layout: ") + error;
    return {};
}

void InputConfig::applyPointer(bool touchpad, const QString &key, const QVariant &value)
{
    xcb_connection_t *c = Platform::xConnection();
    const PointerProperty *property = nullptr;
    for (const PointerProperty &p : Properties) {
        if (key == QLatin1String(p.key))
            property = &p;
    }
    if (!c || !property)
        return;
    const xcb_atom_t atom = Platform::xAtom(property->atom, true);
    for (const Device &device : pointerDevices(c)) {
        if (device.touchpad != touchpad)
            continue;
        const RawProperty current = readProperty(c, device.id, atom);
        if (current.items == 0)
            continue;
        if (property->isFloat) {
            const float f = float(std::clamp(value.toDouble(), -1.0, 1.0));
            xcb_input_xi_change_property(c, device.id, XCB_PROP_MODE_REPLACE, 32, atom, current.type, 1, &f);
        } else if (key == QLatin1String("flatProfile")) {
            QByteArray profile = current.data;     // [adaptive, flat(, custom)]
            profile.fill(0);
            profile[value.toBool() ? 1 : 0] = 1;
            xcb_input_xi_change_property(c, device.id, XCB_PROP_MODE_REPLACE, 8, atom, current.type, current.items, profile.constData());
        } else {
            const uint8_t on = value.toBool() ? 1 : 0;
            xcb_input_xi_change_property(c, device.id, XCB_PROP_MODE_REPLACE, 8, atom, current.type, 1, &on);
        }
    }
    xcb_flush(c);
}

void InputConfig::applySaved()
{
    NoirConfig *config = NoirConfig::instance();
    if (config->value(QStringLiteral("Input/Keyboard/RepeatDelay")).isValid())
        applyRepeat(config->value(QStringLiteral("Input/Keyboard/RepeatEnabled"), true).toBool(),
                    config->value(QStringLiteral("Input/Keyboard/RepeatDelay")).toInt(),
                    config->value(QStringLiteral("Input/Keyboard/RepeatRate"), 25).toInt());
    const QString layouts = config->value(QStringLiteral("Input/Keyboard/Layouts")).toString();
    if (!layouts.isEmpty())
        applyLayouts(layouts.split(QLatin1Char(',')), config->value(QStringLiteral("Input/Keyboard/LayoutSwitch")).toString());
    for (bool touchpad : { false, true }) {
        for (const PointerProperty &p : Properties) {
            const QVariant value = config->value(configKey(touchpad, QLatin1String(p.key)));
            if (value.isValid())
                applyPointer(touchpad, QLatin1String(p.key), value);
        }
    }
}
