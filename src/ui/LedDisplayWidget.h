#pragma once

#include <QImage>
#include <QMetaObject>
#include <QPixmap>
#include <QString>
#include <QWidget>

namespace d05 {

// The display slot of the JP-08 and JU-06A skins (DESIGN-2.md 4.2): the unit's own
// seven-segment readout (two or three cells, from the skin) in a black window, with the
// patch name and one context line in panel lettering beside it.
//
// Semantic content in, pixels out: setContent() compares with what it holds and only then
// redraws the parts that changed into one cached frame and calls update(). The segments come
// from a pre-rendered atlas (13 characters, each with and without the lit decimal point),
// built with antialiasing once per (P, skin, ghost switch) and blitted unscaled. paintEvent
// draws the cached frame and nothing else: no layout, no allocation.
//
// The widget reports the same size as DisplayWidget at the same step, device pixel ratio and
// window width, so nothing below it moves when the skin changes.
class LedDisplayWidget : public QWidget {
    Q_OBJECT
public:
    // The colour of the context line (DESIGN-2.md 4.2): labelSecondary, labelPrimary, labelRed,
    // or ledSegOn (the prompt belongs to the unit).
    enum class Tone { Secondary, Primary, Red, Lit };

    // The characters the readout draws, in atlas order.
    static const char *characters() { return "0123456789-_ "; }
    static constexpr int CharacterCount = 13;

    explicit LedDisplayWidget(QWidget *parent = nullptr);

    // Step p = 3, 4 or 5 and the window width, exactly as DisplayWidget::setStep.
    void setStep(int p, int windowWidth);
    int pitch() const { return m_pitch; }

    // Readout of `cells()` characters (shorter is padded, longer cut; characters outside
    // 0-9 - _ and space draw blank), the name (blank for none), the context line and its tone.
    // Does nothing, and does not repaint, when nothing changed. The decimal point is never lit:
    // the app cannot know that a patch is being edited.
    void setContent(const QString &readout, const QString &name, const QString &context, Tone tone);
    QString readout() const { return m_readout; }
    QString name() const { return m_name; }
    QString context() const { return m_context; }
    Tone tone() const { return m_tone; }

    // Cell count from the current skin (ledCells); rebuilt on a skin change.
    int cells() const { return m_cells; }

    // Geometry, logical px (DESIGN-2.md 4.2). The window is (0, 0, windowWidth, height()).
    int windowWidth() const { return m_windowW; }
    int fieldWidth() const { return m_windowW - 22; }
    // The baselines the name and the context are drawn on, and their left edge.
    int nameBaseline() const { return m_nameBaseline; }
    int contextBaseline() const { return m_contextBaseline; }
    int textLeft() const { return m_windowW + 24; }
    int textWidth() const { return width() - m_windowW - 24 - 8; }

    // Device-pixel dot pitch P of the readout (tests).
    int deviceDot() const { return m_P; }
    // How many times the frame / the atlas were built (tests: painting and unchanged content
    // must build nothing).
    int frameBuildCount() const { return m_frameBuilds; }
    static int atlasBuildCount();
    // The atlas: CharacterCount x 2 cells of 11P x 15P, character c without the dot at column
    // 2c and with it at 2c + 1 (tests).
    const QImage &atlas() const { return m_atlas; }
    // The widget as the frame holds it now (tests).
    QImage frameImage() const { return m_frame.toImage(); }
    // Test hooks: pretend the widget's DPR is `dpr` (0 = the real one); the second also rebuilds.
    // -1 = the skin's ledShowGhost, 0 / 1 = force it (no skin turns the ghost segments off today).
    void overrideGhostForTest(int ghost);
    void overrideDprForTest(qreal dpr);
    void rebuildForDprForTest(qreal dpr);

    QSize sizeHint() const override { return size(); }

signals:
    // The widget's fixed size changed (a DPR or screen change, a new step): the window must lay
    // out again even though it was not resized.
    void sizeChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool event(QEvent *event) override;

private:
    qreal currentDpr() const;
    bool resourcesStale() const;
    void ensureResources();
    void buildAtlas();
    void buildFrame();
    void paintCell(QPainter &p, int cell, int character);
    void paintText(QPainter &p, bool name, bool context, const QSize &logicalSize);
    QString normalised(const QString &readout) const;
    void watchWindow();

    int m_pitch = 3;
    int m_windowWidth = 100000;
    QString m_readout;
    QString m_name;
    QString m_context;
    Tone m_tone = Tone::Secondary;

    // Resources, keyed by (P, dpr, skin generation).
    qreal m_dprOverride = 0;
    int m_ghostOverride = -1;
    bool ghostOn() const;
    bool m_rebuildQueued = false;
    QMetaObject::Connection m_screenConn;
    qreal m_dpr = 0;
    int m_P = 0;
    int m_cells = 0;
    int m_pitchBuilt = 0;
    int m_skinGeneration = 0;       // bumped by the Theme notifier
    int m_skinGenerationBuilt = -1;
    int m_atlasP = 0;
    int m_atlasSkinGeneration = -1;
    bool m_atlasGhost = false;
    int m_windowW = 0;      // logical
    int m_nameBaseline = 0;
    int m_contextBaseline = 0;
    int m_splitY = 0;       // logical y between the name strip and the context strip
    int m_fieldX = 0;       // device px of the field's top-left
    int m_fieldY = 0;
    int m_frameBuilds = 0;
    QImage m_atlas;
    QPixmap m_frame;
};

}  // namespace d05
