#pragma once

#include "core/PatchAddress.h"

#include <QImage>
#include <QPixmap>
#include <QString>
#include <QMetaObject>
#include <QTimer>
#include <QWidget>

namespace d05 {

// The two-line, 40-column dot-matrix display (DESIGN.md section 5, SPEC FR-12).
//
// Semantic state in, text out: the owner sets the current patch, the connection,
// the search state, an optional entry echo and calls flashSent(); the widget works
// out the two lines (FR-12 priority) and only touches pixels for cells whose
// character changed. The dots come from a pre-rendered glyph atlas, rebuilt only
// when the pitch, the device pixel ratio or the ghost switch changes. paintEvent
// draws the bezel and blits the cached field; it allocates nothing.
class DisplayWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int pitch READ pitch WRITE setPitch)
    Q_PROPERTY(bool lcdShowGhost READ lcdShowGhost WRITE setLcdShowGhost)
    Q_PROPERTY(bool keyModeOnLine2 READ keyModeOnLine2 WRITE setKeyModeOnLine2)
    Q_PROPERTY(bool displayEntryEcho READ displayEntryEcho WRITE setDisplayEntryEcho)
public:
    static constexpr int Columns = 40;
    static constexpr int Lines = 2;
    static constexpr int SentFlashMs = 400;

    explicit DisplayWidget(QWidget *parent = nullptr);

    // Dot pitch in logical px: 3, 4 or 5 (other values are clamped to that range).
    int pitch() const { return m_pitch; }
    void setPitch(int p);  // step only; the window width is left as it was (default: unconstrained)
    // Step and the window's width together (DESIGN 5.1); the widget's size follows
    // (read sizeHint() or size() for bezelW x bezelH).
    void setStep(int p, int windowWidth);
    // The pitch for a window width (DESIGN 4.3): 3 below 1042, 4 below 1285, else 5.
    static int pitchForWidth(int windowWidth);

    // Switches (DESIGN 2.2, 13.3).
    bool lcdShowGhost() const { return m_showGhost; }
    void setLcdShowGhost(bool on);
    bool keyModeOnLine2() const { return m_keyModeOnLine2; }
    void setKeyModeOnLine2(bool on);
    bool displayEntryEcho() const { return m_entryEcho; }
    void setDisplayEntryEcho(bool on);

    // Semantic state. `name` is PatchCatalog::name (empty for an unnamed user slot),
    // `mode` is 'W', 'D', 'S' or a null QChar.
    void setPatch(const PatchAddress &address, const QString &name, QChar mode);
    void setConnected(bool connected);
    // Search active with `matchCount` results (FR-12 priority 3); false = no search.
    void setSearch(bool active, int matchCount);
    // The pending grid entry, e.g. "P3-__" or "P3-4_"; empty when none.
    void setEntry(const QString &text);

    // Shows SENT on line 2 for 400 ms (single-shot timer). Restarts if already on.
    void flashSent();
    bool sentActive() const { return m_sent; }
    // Lights the status-bar activity LED for the same 400 ms (an incoming follow message,
    // DESIGN 8.1) without touching line 2. Shares the one single-shot timer.
    void flashActivity();
    bool activityActive() const { return m_sent || m_activity; }

    // The two lines as stored: always 40 characters each.
    QString line1() const { return m_text[0]; }
    QString line2() const { return m_text[1]; }
    // Sets both lines directly (padded or cut to 40 columns). Does nothing, and
    // does not repaint, when the text is unchanged. Normally called via the
    // semantic setters.
    void setLines(const QString &line1, const QString &line2);

    // "USER 1" ... "USER 8", "PRESET 1: ORIGINAL D-50" ... (group index 0..13).
    static QString groupLongName(int group);
    // "WHOLE", "DUAL", "SPLIT" for 'W', 'D', 'S'; empty otherwise.
    static QString modeText(QChar mode);

    // How many times the lit-field frame was rebuilt (tests: a drag must not rebuild it).
    int frameBuildCount() const { return m_frameBuilds; }

    // Geometry (device pixels), exposed for tests.
    int deviceDot() const { return m_P; }
    // The device-pixel dot pitch P for a logical pitch and a DPR. The single place
    // the rule lives (DESIGN 5.1); everything else derives from it.
    // `windowWidth` is the main window's width (the rule only grows P when it fits).
    static int devicePitch(int logicalPitch, qreal dpr, int windowWidth);
    // The dot gap g: clamp(round(dpr), 1, P / 3).
    static int deviceGap(int P, qreal dpr);
    int deviceGap() const { return m_g; }
    // Test hooks: pretend the widget's DPR is `dpr` (0 = use the real one).
    // overrideDprForTest only records it (the next paint notices the mismatch);
    // rebuildForDprForTest also rebuilds at once.
    void overrideDprForTest(qreal dpr);
    void rebuildForDprForTest(qreal dpr);
    // How many glyph atlases have been built in this process (tests).
    static int atlasBuildCount();
    // The atlas of the current key: 95 glyph cells of 5P x 7P in one row (tests).
    const QImage &atlas() const { return m_atlas; }
    // The rendered lit field as the display holds it now (tests).
    QImage fieldImage() const { return m_frame.toImage(); }

    QSize sizeHint() const override { return size(); }

signals:
    // Emitted when the SENT flash or an activity flash starts (true) and ends (false);
    // the status bar activity LED follows it.
    void sentChanged(bool on);
    // The widget's fixed size changed (a DPR or screen change, a new pitch): the window
    // must lay out again even though it was not resized.
    void sizeChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool event(QEvent *event) override;

private:
    void recompute();
    void ensureResources();
    qreal currentDpr() const;
    bool resourcesStale() const;
    void watchWindow();
    void buildAtlas();
    void paintCell(QPainter &p, int line, int col, ushort ch);
    QString normalised(const QString &s) const;

    int m_pitch = 3;
    bool m_showGhost = true;
    bool m_keyModeOnLine2 = false;
    bool m_entryEcho = true;

    // Semantic state.
    PatchAddress m_address;
    QString m_name;
    QChar m_mode;
    bool m_connected = false;
    bool m_searchActive = false;
    int m_matchCount = 0;
    QString m_entry;
    bool m_sent = false;
    bool m_activity = false;
    QTimer m_sentTimer;

    QString m_text[Lines];

    // Device-pixel resources, keyed by (P, g, ghost, dpr).
    qreal m_dprOverride = 0;
    bool m_rebuildQueued = false;
    QMetaObject::Connection m_screenConn;
    qreal m_dpr = 0;
    int m_P = 0;
    int m_g = 0;
    bool m_builtGhost = false;
    int m_pitchBuilt = 0;
    int m_windowWidth = 100000;
    int m_frameBuilds = 0;
    int m_atlasP = 0;
    int m_atlasG = 0;
    int m_dotOffX = 0;  // device px from the frame's left to the dot area
    int m_dotOffY = 0;
    QImage m_atlas;
    QPixmap m_frame;
};

}  // namespace d05
