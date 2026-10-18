#include <QSettings>
#include <QtGlobal>

#include <cmath>
#include <cstdio>

extern "C" void compositor_motion_settings(int *reduced, float *scale)
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("SiriusOS"), QStringLiteral("Noir"));
    settings.sync();
    if (settings.status() != QSettings::NoError)
        std::fprintf(stderr, "NoirCompositor: cannot read motion preferences\n");
    *reduced = settings.value(QStringLiteral("Appearance/ReducedMotion"), false).toBool();
    bool ok = false;
    const double value = settings.value(QStringLiteral("Appearance/AnimationScale"), 1.0).toDouble(&ok);
    if (!ok || !std::isfinite(value)) {
        std::fprintf(stderr, "NoirCompositor: invalid animation scale; using 1.0\n");
        *scale = 1.0f;
    } else {
        *scale = float(qBound(0.25, value, 3.0));
    }
}
