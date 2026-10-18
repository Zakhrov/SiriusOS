#pragma once

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

#include <xcb/randr.h>

// Display page: monitors through RandR (resolution, refresh rate, rotation,
// on/off, primary) and backlight brightness through logind.
//
// A layout is a list of {name, enabled, width, height, refresh, rotation,
// primary} maps, one per connected output. Enabled outputs are placed side
// by side, left to right, in the order of their current positions.
class DisplayConfig : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QVariantList outputs READ outputs NOTIFY changed)
    Q_PROPERTY(QVariantList backlights READ backlights NOTIFY backlightsChanged)
    Q_PROPERTY(QString screenSize READ screenSize NOTIFY changed)

public:
    explicit DisplayConfig(QObject *parent = nullptr);

    bool available() const { return m_available; }
    QVariantList outputs() const { return m_outputList; }
    QVariantList backlights() const;
    QString screenSize() const { return m_screenSize; }

    // `probe` asks the X server to look for newly connected monitors, which
    // can take a moment.
    Q_INVOKABLE void refresh(bool probe = false);
    Q_INVOKABLE QVariantList currentLayout() const;
    // Returns "" on success, otherwise what went wrong.
    Q_INVOKABLE QString applyLayout(const QVariantList &layout);
    Q_INVOKABLE void saveLayout(const QVariantList &layout);
    // Applies the saved layout for the monitors that are connected (login).
    Q_INVOKABLE QString applySaved();
    Q_INVOKABLE void setBrightness(const QString &name, double fraction);

signals:
    void changed();
    void backlightsChanged();
    void errorOccurred(const QString &message);

private:
    struct Mode {
        xcb_randr_mode_t id;
        int width, height;
        double refresh;
    };
    struct Output {
        xcb_randr_output_t id;
        QString name;
        bool connected = false;
        xcb_randr_crtc_t crtc = 0;
        QList<xcb_randr_crtc_t> crtcs;
        QList<xcb_randr_mode_t> modes;
        int preferred = 0;          // the first `preferred` modes are preferred
        int x = 0, y = 0;
        xcb_randr_mode_t mode = 0;
        int rotation = 0;           // degrees
        int mmWidth = 0, mmHeight = 0;
    };
    struct Crtc {
        int x = 0, y = 0;
        xcb_randr_mode_t mode = 0;
        uint16_t rotation = XCB_RANDR_ROTATION_ROTATE_0;
        QList<xcb_randr_output_t> outputs;
    };

    QVariantMap describe(const Output &output, bool primary) const;

    bool m_available = false;
    xcb_timestamp_t m_configTimestamp = 0;
    QHash<xcb_randr_mode_t, Mode> m_modes;
    QList<Output> m_outputs;
    QHash<xcb_randr_crtc_t, Crtc> m_crtcs;
    xcb_randr_output_t m_primary = 0;
    QVariantList m_outputList;
    QString m_screenSize;
    int m_minWidth = 0, m_minHeight = 0, m_maxWidth = 0, m_maxHeight = 0;
};
