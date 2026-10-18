#include "DisplayConfig.h"

#include "NoirConfig.h"
#include "Platform.h"

#include <QDBusConnection>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QSize>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace {

double modeRefresh(const xcb_randr_mode_info_t &m)
{
    double vtotal = m.vtotal;
    if (m.mode_flags & XCB_RANDR_MODE_FLAG_DOUBLE_SCAN)
        vtotal *= 2;
    if (m.mode_flags & XCB_RANDR_MODE_FLAG_INTERLACE)
        vtotal /= 2;
    return m.htotal && vtotal ? double(m.dot_clock) / (double(m.htotal) * vtotal) : 0.0;
}

uint16_t rotationBits(int degrees)
{
    switch (((degrees % 360) + 360) % 360) {
    case 90: return XCB_RANDR_ROTATION_ROTATE_90;
    case 180: return XCB_RANDR_ROTATION_ROTATE_180;
    case 270: return XCB_RANDR_ROTATION_ROTATE_270;
    default: return XCB_RANDR_ROTATION_ROTATE_0;
    }
}

int rotationDegrees(uint16_t bits)
{
    if (bits & XCB_RANDR_ROTATION_ROTATE_90) return 90;
    if (bits & XCB_RANDR_ROTATION_ROTATE_180) return 180;
    if (bits & XCB_RANDR_ROTATION_ROTATE_270) return 270;
    return 0;
}

// The screen's size now. (The connection setup's width_in_pixels is a
// snapshot from when the connection was made, so it goes stale on RandR
// changes.)
QSize rootSize(xcb_connection_t *c, xcb_window_t root)
{
    xcb_get_geometry_reply_t *geometry = xcb_get_geometry_reply(c, xcb_get_geometry(c, root), nullptr);
    const QSize size = geometry ? QSize(geometry->width, geometry->height) : QSize();
    free(geometry);
    return size;
}

QString readSysfs(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromLatin1(file.readAll()).trimmed() : QString();
}

} // namespace

DisplayConfig::DisplayConfig(QObject *parent)
    : QObject(parent)
{
    refresh();
}

void DisplayConfig::refresh(bool probe)
{
    m_available = false;
    m_modes.clear();
    m_outputs.clear();
    m_crtcs.clear();
    m_outputList.clear();

    xcb_connection_t *c = Platform::xConnection();
    xcb_screen_t *screen = Platform::xScreen();
    if (!c || !screen || !Platform::hasExtension(&xcb_randr_id)) {
        emit changed();
        return;
    }
    xcb_randr_query_version_reply_t *version = xcb_randr_query_version_reply(c, xcb_randr_query_version(c, 1, 5), nullptr);
    m_available = version && (version->major_version > 1 || version->minor_version >= 2);
    free(version);
    if (!m_available) {
        emit changed();
        return;
    }

    if (auto *range = xcb_randr_get_screen_size_range_reply(c, xcb_randr_get_screen_size_range(c, screen->root), nullptr)) {
        m_minWidth = range->min_width; m_minHeight = range->min_height;
        m_maxWidth = range->max_width; m_maxHeight = range->max_height;
        free(range);
    }

    // The "current" variant answers from the server's cache; the full one
    // probes the hardware for monitors plugged in since.
    xcb_randr_get_screen_resources_current_reply_t *res = nullptr;
    xcb_randr_get_screen_resources_reply_t *probed = nullptr;
    const xcb_randr_mode_info_t *modeInfos;
    const uint8_t *names;
    const xcb_randr_output_t *outputIds;
    const xcb_randr_crtc_t *crtcIds;
    int modeCount, outputCount, crtcCount;
    if (probe) {
        probed = xcb_randr_get_screen_resources_reply(c, xcb_randr_get_screen_resources(c, screen->root), nullptr);
        if (!probed) { m_available = false; emit changed(); return; }
        m_configTimestamp = probed->config_timestamp;
        modeInfos = xcb_randr_get_screen_resources_modes(probed); modeCount = xcb_randr_get_screen_resources_modes_length(probed);
        names = xcb_randr_get_screen_resources_names(probed);
        outputIds = xcb_randr_get_screen_resources_outputs(probed); outputCount = xcb_randr_get_screen_resources_outputs_length(probed);
        crtcIds = xcb_randr_get_screen_resources_crtcs(probed); crtcCount = xcb_randr_get_screen_resources_crtcs_length(probed);
    } else {
        res = xcb_randr_get_screen_resources_current_reply(c, xcb_randr_get_screen_resources_current(c, screen->root), nullptr);
        if (!res) { m_available = false; emit changed(); return; }
        m_configTimestamp = res->config_timestamp;
        modeInfos = xcb_randr_get_screen_resources_current_modes(res); modeCount = xcb_randr_get_screen_resources_current_modes_length(res);
        names = xcb_randr_get_screen_resources_current_names(res);
        outputIds = xcb_randr_get_screen_resources_current_outputs(res); outputCount = xcb_randr_get_screen_resources_current_outputs_length(res);
        crtcIds = xcb_randr_get_screen_resources_current_crtcs(res); crtcCount = xcb_randr_get_screen_resources_current_crtcs_length(res);
    }
    Q_UNUSED(names);

    for (int i = 0; i < modeCount; ++i) {
        const xcb_randr_mode_info_t &m = modeInfos[i];
        m_modes.insert(m.id, Mode { m.id, m.width, m.height, modeRefresh(m) });
    }
    for (int i = 0; i < crtcCount; ++i) {
        xcb_randr_get_crtc_info_reply_t *info = xcb_randr_get_crtc_info_reply(
            c, xcb_randr_get_crtc_info(c, crtcIds[i], m_configTimestamp), nullptr);
        if (!info)
            continue;
        Crtc crtc;
        crtc.x = info->x; crtc.y = info->y; crtc.mode = info->mode; crtc.rotation = info->rotation;
        const xcb_randr_output_t *outs = xcb_randr_get_crtc_info_outputs(info);
        for (int k = 0; k < xcb_randr_get_crtc_info_outputs_length(info); ++k)
            crtc.outputs << outs[k];
        m_crtcs.insert(crtcIds[i], crtc);
        free(info);
    }
    for (int i = 0; i < outputCount; ++i) {
        xcb_randr_get_output_info_reply_t *info = xcb_randr_get_output_info_reply(
            c, xcb_randr_get_output_info(c, outputIds[i], m_configTimestamp), nullptr);
        if (!info)
            continue;
        Output out;
        out.id = outputIds[i];
        out.name = QString::fromUtf8(reinterpret_cast<const char *>(xcb_randr_get_output_info_name(info)),
                                     xcb_randr_get_output_info_name_length(info));
        out.connected = info->connection == XCB_RANDR_CONNECTION_CONNECTED;
        out.crtc = info->crtc;
        out.mmWidth = int(info->mm_width);
        out.mmHeight = int(info->mm_height);
        out.preferred = info->num_preferred;
        const xcb_randr_crtc_t *crtcs = xcb_randr_get_output_info_crtcs(info);
        for (int k = 0; k < xcb_randr_get_output_info_crtcs_length(info); ++k)
            out.crtcs << crtcs[k];
        const xcb_randr_mode_t *modes = xcb_randr_get_output_info_modes(info);
        for (int k = 0; k < xcb_randr_get_output_info_modes_length(info); ++k)
            out.modes << modes[k];
        if (out.crtc && m_crtcs.contains(out.crtc)) {
            const Crtc &crtc = m_crtcs.value(out.crtc);
            out.x = crtc.x; out.y = crtc.y; out.mode = crtc.mode;
            out.rotation = rotationDegrees(crtc.rotation);
        }
        free(info);
        m_outputs << out;
    }
    free(res);
    free(probed);

    if (auto *primary = xcb_randr_get_output_primary_reply(c, xcb_randr_get_output_primary(c, screen->root), nullptr)) {
        m_primary = primary->output;
        free(primary);
    }

    // Connected outputs first, then by position.
    std::stable_sort(m_outputs.begin(), m_outputs.end(), [](const Output &a, const Output &b) {
        if (a.connected != b.connected) return a.connected;
        return a.x < b.x;
    });
    for (const Output &out : m_outputs) {
        if (out.connected)
            m_outputList << describe(out, out.id == m_primary);
    }
    const QSize size = rootSize(c, screen->root);
    m_screenSize = QStringLiteral("%1 × %2").arg(size.width()).arg(size.height());
    emit changed();
}

QVariantMap DisplayConfig::describe(const Output &out, bool primary) const
{
    // Group the output's modes by size; each size lists its refresh rates.
    QList<QPair<int, int>> sizes;
    QMap<QPair<int, int>, QList<double>> rates;
    for (int i = 0; i < out.modes.size(); ++i) {
        const Mode m = m_modes.value(out.modes.at(i));
        const QPair<int, int> size(m.width, m.height);
        if (!rates.contains(size))
            sizes << size;
        QList<double> &list = rates[size];
        const double hz = std::round(m.refresh * 100) / 100;
        if (std::none_of(list.begin(), list.end(), [hz](double r) { return std::abs(r - hz) < 0.01; }))
            list << hz;
    }
    std::sort(sizes.begin(), sizes.end(), [](const QPair<int, int> &a, const QPair<int, int> &b) {
        return a.first * a.second > b.first * b.second;
    });
    QVariantList resolutions;
    for (const auto &size : sizes) {
        QList<double> list = rates.value(size);
        std::sort(list.begin(), list.end(), std::greater<double>());
        QVariantList rateList;
        for (double r : list)
            rateList << r;
        resolutions << QVariantMap {
            { QStringLiteral("width"), size.first },
            { QStringLiteral("height"), size.second },
            { QStringLiteral("rates"), rateList },
        };
    }
    const Mode current = m_modes.value(out.mode);
    const Mode preferred = out.preferred > 0 && !out.modes.isEmpty() ? m_modes.value(out.modes.first()) : Mode {};
    QString physical;
    if (out.mmWidth > 0 && out.mmHeight > 0) {
        const double inches = std::sqrt(double(out.mmWidth) * out.mmWidth + double(out.mmHeight) * out.mmHeight) / 25.4;
        physical = QStringLiteral("%1″ (%2 × %3 mm)").arg(inches, 0, 'f', 1).arg(out.mmWidth).arg(out.mmHeight);
    }
    return {
        { QStringLiteral("name"), out.name },
        { QStringLiteral("enabled"), out.mode != 0 },
        { QStringLiteral("primary"), primary },
        { QStringLiteral("x"), out.x },
        { QStringLiteral("y"), out.y },
        { QStringLiteral("width"), current.width },
        { QStringLiteral("height"), current.height },
        { QStringLiteral("refresh"), std::round(current.refresh * 100) / 100 },
        { QStringLiteral("rotation"), out.rotation },
        { QStringLiteral("preferredWidth"), preferred.width },
        { QStringLiteral("preferredHeight"), preferred.height },
        { QStringLiteral("resolutions"), resolutions },
        { QStringLiteral("physicalSize"), physical },
    };
}

QVariantList DisplayConfig::currentLayout() const
{
    QVariantList layout;
    for (const QVariant &v : m_outputList) {
        const QVariantMap o = v.toMap();
        layout << QVariantMap {
            { QStringLiteral("name"), o.value(QStringLiteral("name")) },
            { QStringLiteral("enabled"), o.value(QStringLiteral("enabled")) },
            { QStringLiteral("width"), o.value(QStringLiteral("width")) },
            { QStringLiteral("height"), o.value(QStringLiteral("height")) },
            { QStringLiteral("refresh"), o.value(QStringLiteral("refresh")) },
            { QStringLiteral("rotation"), o.value(QStringLiteral("rotation")) },
            { QStringLiteral("primary"), o.value(QStringLiteral("primary")) },
        };
    }
    return layout;
}

QString DisplayConfig::applyLayout(const QVariantList &layout)
{
    xcb_connection_t *c = Platform::xConnection();
    xcb_screen_t *screen = Platform::xScreen();
    if (!m_available || !c || !screen)
        return QStringLiteral("The X server does not support display configuration (RandR 1.2).");
    refresh();   // work from the server's current state and timestamp

    struct Target {
        const Output *output;
        bool enabled = false;
        xcb_randr_mode_t mode = 0;
        uint16_t rotation = XCB_RANDR_ROTATION_ROTATE_0;
        int width = 0, height = 0;     // after rotation
        int x = 0, y = 0;
        xcb_randr_crtc_t crtc = 0;
        bool primary = false;
    };
    QList<Target> targets;
    for (const QVariant &v : layout) {
        const QVariantMap cfg = v.toMap();
        const QString name = cfg.value(QStringLiteral("name")).toString();
        auto it = std::find_if(m_outputs.cbegin(), m_outputs.cend(), [&](const Output &o) { return o.name == name; });
        if (it == m_outputs.cend() || !it->connected)
            continue;
        Target t;
        t.output = &*it;
        t.enabled = cfg.value(QStringLiteral("enabled"), true).toBool();
        t.primary = cfg.value(QStringLiteral("primary")).toBool();
        if (t.enabled) {
            const int w = cfg.value(QStringLiteral("width")).toInt();
            const int h = cfg.value(QStringLiteral("height")).toInt();
            const double hz = cfg.value(QStringLiteral("refresh")).toDouble();
            double best = 1e9;
            for (xcb_randr_mode_t id : it->modes) {
                const Mode m = m_modes.value(id);
                if (m.width != w || m.height != h)
                    continue;
                const double distance = hz > 0 ? std::abs(m.refresh - hz) : -m.refresh;
                if (distance < best) { best = distance; t.mode = id; }
            }
            if (!t.mode)
                return QStringLiteral("%1 has no %2 × %3 mode.").arg(name).arg(w).arg(h);
            const int degrees = cfg.value(QStringLiteral("rotation")).toInt();
            t.rotation = rotationBits(degrees);
            const bool sideways = degrees == 90 || degrees == 270;
            t.width = sideways ? h : w;
            t.height = sideways ? w : h;
        }
        targets << t;
    }
    if (std::none_of(targets.begin(), targets.end(), [](const Target &t) { return t.enabled; }))
        return QStringLiteral("At least one display has to stay on.");

    // Left to right, in the current order; newly enabled outputs go last.
    QList<Target *> order;
    for (Target &t : targets) {
        if (t.enabled)
            order << &t;
    }
    std::stable_sort(order.begin(), order.end(), [](const Target *a, const Target *b) {
        const bool aOn = a->output->mode != 0, bOn = b->output->mode != 0;
        if (aOn != bOn) return aOn;
        return a->output->x < b->output->x;
    });
    int screenWidth = 0, screenHeight = 0;
    for (Target *t : order) {
        t->x = screenWidth;
        t->y = 0;
        screenWidth += t->width;
        screenHeight = std::max(screenHeight, t->height);
    }
    if (screenWidth > m_maxWidth || screenHeight > m_maxHeight)
        return QStringLiteral("The combined size %1 × %2 exceeds what the graphics hardware supports (%3 × %4).")
            .arg(screenWidth).arg(screenHeight).arg(m_maxWidth).arg(m_maxHeight);
    screenWidth = std::max(screenWidth, m_minWidth);
    screenHeight = std::max(screenHeight, m_minHeight);

    // CRTCs: keep each output's own, give newly enabled ones a free one.
    QSet<xcb_randr_crtc_t> used;
    for (Target &t : targets) {
        if (t.enabled && t.output->crtc) {
            t.crtc = t.output->crtc;
            used.insert(t.crtc);
        }
    }
    for (Target &t : targets) {
        if (!t.enabled || t.crtc)
            continue;
        for (xcb_randr_crtc_t candidate : t.output->crtcs) {
            const Crtc crtc = m_crtcs.value(candidate);
            const bool busy = used.contains(candidate)
                || (crtc.mode != 0 && std::none_of(targets.begin(), targets.end(), [&](const Target &o) {
                       return o.output->crtc == candidate && !o.enabled; }));
            if (!busy) { t.crtc = candidate; used.insert(candidate); break; }
        }
        if (!t.crtc)
            return QStringLiteral("No free display controller for %1.").arg(t.output->name);
    }

    auto unchanged = [&](const Target &t) {
        const Crtc crtc = m_crtcs.value(t.crtc);
        return crtc.mode == t.mode && crtc.rotation == t.rotation && crtc.x == t.x && crtc.y == t.y
            && crtc.outputs == QList<xcb_randr_output_t> { t.output->id };
    };
    QString error;
    auto check = [&](xcb_randr_set_crtc_config_cookie_t cookie, const QString &what) {
        xcb_generic_error_t *e = nullptr;
        xcb_randr_set_crtc_config_reply_t *reply = xcb_randr_set_crtc_config_reply(c, cookie, &e);
        if ((!reply || reply->status != XCB_RANDR_SET_CONFIG_SUCCESS) && error.isEmpty())
            error = QStringLiteral("The X server refused to configure %1.").arg(what);
        free(reply);
        free(e);
    };

    xcb_grab_server(c);
    // 1. Switch off every CRTC that changes or goes away, so the screen can shrink.
    for (const Target &t : targets) {
        const xcb_randr_crtc_t crtc = t.enabled ? t.crtc : t.output->crtc;
        if (!crtc || (t.enabled && unchanged(t)) || m_crtcs.value(crtc).mode == 0)
            continue;
        check(xcb_randr_set_crtc_config(c, crtc, XCB_CURRENT_TIME, m_configTimestamp, 0, 0, XCB_NONE,
                                        XCB_RANDR_ROTATION_ROTATE_0, 0, nullptr), t.output->name);
    }
    // 2. Resize the screen, keeping 96 dpi for the physical size it reports.
    const QSize current = rootSize(c, screen->root);
    if (screenWidth != current.width() || screenHeight != current.height()) {
        xcb_randr_set_screen_size(c, screen->root, uint16_t(screenWidth), uint16_t(screenHeight),
                                  uint32_t(std::lround(screenWidth * 25.4 / 96)), uint32_t(std::lround(screenHeight * 25.4 / 96)));
    }
    // 3. Switch the changed CRTCs on in their new configuration.
    for (const Target &t : targets) {
        if (!t.enabled || unchanged(t))
            continue;
        const xcb_randr_output_t output = t.output->id;
        check(xcb_randr_set_crtc_config(c, t.crtc, XCB_CURRENT_TIME, m_configTimestamp, int16_t(t.x), int16_t(t.y),
                                        t.mode, t.rotation, 1, &output), t.output->name);
    }
    for (const Target &t : targets) {
        if (t.enabled && t.primary)
            xcb_randr_set_output_primary(c, screen->root, t.output->id);
    }
    xcb_ungrab_server(c);
    xcb_flush(c);

    refresh();
    if (!error.isEmpty())
        emit errorOccurred(error);
    return error;
}

void DisplayConfig::saveLayout(const QVariantList &layout)
{
    NoirConfig *config = NoirConfig::instance();
    for (const QVariant &v : layout) {
        const QVariantMap o = v.toMap();
        const QString group = QStringLiteral("Display/") + o.value(QStringLiteral("name")).toString() + QLatin1Char('/');
        config->setValue(group + QStringLiteral("Enabled"), o.value(QStringLiteral("enabled")).toBool());
        config->setValue(group + QStringLiteral("Width"), o.value(QStringLiteral("width")).toInt());
        config->setValue(group + QStringLiteral("Height"), o.value(QStringLiteral("height")).toInt());
        config->setValue(group + QStringLiteral("Refresh"), o.value(QStringLiteral("refresh")).toDouble());
        config->setValue(group + QStringLiteral("Rotation"), o.value(QStringLiteral("rotation")).toInt());
        config->setValue(group + QStringLiteral("Primary"), o.value(QStringLiteral("primary")).toBool());
    }
}

QString DisplayConfig::applySaved()
{
    refresh(true);
    NoirConfig *config = NoirConfig::instance();
    const QStringList saved = config->childGroups(QStringLiteral("Display"));
    QVariantList layout = currentLayout();
    bool any = false;
    for (QVariant &v : layout) {
        QVariantMap o = v.toMap();
        const QString name = o.value(QStringLiteral("name")).toString();
        if (!saved.contains(name))
            continue;
        const QString group = QStringLiteral("Display/") + name + QLatin1Char('/');
        o[QStringLiteral("enabled")] = config->value(group + QStringLiteral("Enabled"), true);
        o[QStringLiteral("width")] = config->value(group + QStringLiteral("Width"));
        o[QStringLiteral("height")] = config->value(group + QStringLiteral("Height"));
        o[QStringLiteral("refresh")] = config->value(group + QStringLiteral("Refresh"));
        o[QStringLiteral("rotation")] = config->value(group + QStringLiteral("Rotation"), 0);
        o[QStringLiteral("primary")] = config->value(group + QStringLiteral("Primary"), false);
        v = o;
        any = true;
    }
    return any ? applyLayout(layout) : QString();
}

QVariantList DisplayConfig::backlights() const
{
    QVariantList list;
    const QDir dir(QStringLiteral("/sys/class/backlight"));
    for (const QString &name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const double max = readSysfs(dir.filePath(name + QStringLiteral("/max_brightness"))).toDouble();
        const double value = readSysfs(dir.filePath(name + QStringLiteral("/brightness"))).toDouble();
        if (max <= 0)
            continue;
        list << QVariantMap {
            { QStringLiteral("name"), name },
            { QStringLiteral("type"), readSysfs(dir.filePath(name + QStringLiteral("/type"))) },
            { QStringLiteral("value"), value / max },
            { QStringLiteral("max"), max },
        };
    }
    return list;
}

void DisplayConfig::setBrightness(const QString &name, double fraction)
{
    const double max = readSysfs(QStringLiteral("/sys/class/backlight/%1/max_brightness").arg(name)).toDouble();
    if (max <= 0)
        return;
    // Never all the way to black: that looks like a dead screen.
    const uint value = uint(std::lround(std::clamp(fraction, 0.01, 1.0) * max));
    Platform::callAsync(this, QDBusConnection::systemBus(), QStringLiteral("org.freedesktop.login1"),
                        QStringLiteral("/org/freedesktop/login1/session/auto"), QStringLiteral("org.freedesktop.login1.Session"),
                        QStringLiteral("SetBrightness"), { QStringLiteral("backlight"), name, value },
                        [this](const QString &error, const QDBusMessage &) {
                            if (!error.isEmpty())
                                emit errorOccurred(QStringLiteral("Could not change the brightness: ") + error);
                            emit backlightsChanged();
                        });
}
