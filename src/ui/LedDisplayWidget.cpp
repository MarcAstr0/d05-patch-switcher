#include "ui/LedDisplayWidget.h"

#include "ui/DisplayWidget.h"
#include "ui/Theme.h"

#include <QFontMetrics>
#include <QFontMetricsF>
#include <QPaintEvent>
#include <QPainter>
#include <QWindow>

#include <atomic>
#include <cmath>
#include <cstring>

namespace d05 {

namespace {

std::atomic<int> g_atlasBuilds{0};

// DESIGN-2.md 4.2, in digit-box units (8 wide, 15 tall), unslanted. Segment order a b c d e f g.
constexpr int kSegmentCount = 7;
constexpr double kSegments[kSegmentCount][12] = {
    {1.25, 1, 2.25, 0, 5.75, 0, 6.75, 1, 5.75, 2, 2.25, 2},                    // a (top)
    {7, 1.25, 8, 2.25, 8, 6.25, 7, 7.25, 6, 6.25, 6, 2.25},                    // b (upper right)
    {7, 7.75, 8, 8.75, 8, 12.75, 7, 13.75, 6, 12.75, 6, 8.75},                 // c (lower right)
    {1.25, 14, 2.25, 13, 5.75, 13, 6.75, 14, 5.75, 15, 2.25, 15},              // d (bottom)
    {1, 7.75, 2, 8.75, 2, 12.75, 1, 13.75, 0, 12.75, 0, 8.75},                 // e (lower left)
    {1, 1.25, 2, 2.25, 2, 6.25, 1, 7.25, 0, 6.25, 0, 2.25},                    // f (upper left)
    {1.25, 7.5, 2.25, 6.5, 5.75, 6.5, 6.75, 7.5, 5.75, 8.5, 2.25, 8.5},        // g (middle)
};
constexpr double kDot[8] = {8.5, 13, 10.5, 13, 10.5, 15, 8.5, 15};

constexpr double kDigitHeight = 15;
constexpr double kSlant = 0.1;

// Segment masks for "0123456789-_ " (bit 0 = a ... bit 6 = g).
constexpr int bit(char s) { return 1 << (s - 'a'); }
constexpr int mask(const char *s)
{
    int m = 0;
    for (; *s; ++s)
        m |= bit(*s);
    return m;
}
constexpr int kMasks[LedDisplayWidget::CharacterCount] = {
    mask("abcdef"), mask("bc"),   mask("abdeg"), mask("abcdg"),  mask("bcfg"), mask("acdfg"), mask("acdefg"),
    mask("abc"),    mask("abcdefg"), mask("abcdfg"), mask("g"),     mask("d"),    0,
};

int characterIndex(QChar ch)
{
    const char *all = LedDisplayWidget::characters();
    const ushort u = ch.unicode();
    if (u < 0x80 && u != 0) {
        if (const char *at = std::strchr(all, int(u)))
            return int(at - all);
    }
    return LedDisplayWidget::CharacterCount - 1;  // anything else draws blank
}

// One polygon, slanted (every point (x, y) is drawn at (x + 0.1 (15 - y), y)), scaled to device px.
void fillPolygon(QPainter &p, const double *pts, int count, double P, double ox, double oy, const QColor &colour)
{
    QPointF out[6];
    for (int i = 0; i < count; ++i) {
        const double x = pts[2 * i];
        const double y = pts[2 * i + 1];
        out[i] = QPointF(ox + (x + kSlant * (kDigitHeight - y)) * P, oy + y * P);
    }
    p.setBrush(colour);
    p.drawPolygon(out, count);
}

}  // namespace

LedDisplayWidget::LedDisplayWidget(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_OpaquePaintEvent);
    m_cells = Theme::skin().ledCells;
    m_readout = QString(m_cells, QLatin1Char('-'));
    connect(Theme::notifier(), &Theme::Notifier::skinChanged, this, [this] {
        ++m_skinGeneration;
        // A hidden widget, or one whose skin has no LED display, rebuilds when next shown or driven.
        if (isVisible() && Theme::skin().display == Theme::DisplayKind::Led) {
            ensureResources();
            update();
        }
    });
    // No atlas here: showEvent, setStep and setContent build it, so a hidden widget costs nothing.
}

int LedDisplayWidget::atlasBuildCount()
{
    return g_atlasBuilds.load();
}

void LedDisplayWidget::setStep(int p, int windowWidth)
{
    p = qBound(3, p, 5);
    m_pitch = p;
    m_windowWidth = windowWidth;
    if (resourcesStale()) {
        ensureResources();
        update();
    }
}

QString LedDisplayWidget::normalised(const QString &readout) const
{
    QString r = readout.left(m_cells);
    if (r.size() < m_cells)
        r.append(QString(m_cells - r.size(), QLatin1Char(' ')));
    return r;
}

void LedDisplayWidget::setContent(const QString &readout, const QString &name, const QString &context, Tone tone)
{
    // The cell count follows the skin; a pending skin change is applied first so that the
    // readout is cut to the right length.
    if (resourcesStale())
        ensureResources();
    const QString r = normalised(readout);
    const bool readoutChanged = r != m_readout;
    const bool nameChanged = name != m_name;
    const bool contextChanged = context != m_context || tone != m_tone;
    if (!readoutChanged && !nameChanged && !contextChanged)
        return;
    const QString oldReadout = m_readout;
    m_readout = r;
    m_name = name;
    m_context = context;
    m_tone = tone;

    // Cells are placed in device pixels: paint on the pixmap with its DPR neutral.
    m_frame.setDevicePixelRatio(1.0);
    {
        QPainter p(&m_frame);
        if (readoutChanged) {
            for (int k = 0; k < m_cells; ++k) {
                if (r.at(k) != oldReadout.at(k))
                    paintCell(p, k, characterIndex(r.at(k)));
            }
        }
        if (nameChanged || contextChanged)
            paintText(p, nameChanged, contextChanged, size());
    }
    m_frame.setDevicePixelRatio(m_dpr);
    update();
}

qreal LedDisplayWidget::currentDpr() const
{
    return m_dprOverride > 0 ? m_dprOverride : devicePixelRatioF();
}

bool LedDisplayWidget::resourcesStale() const
{
    // As DisplayWidget (DESIGN 5.1): a window width matters only through the pitch it yields.
    return m_P == 0 || !qFuzzyCompare(currentDpr(), m_dpr) || m_pitchBuilt != m_pitch
           || m_skinGenerationBuilt != m_skinGeneration || m_cells != Theme::skin().ledCells
           || DisplayWidget::devicePitch(m_pitch, currentDpr(), m_windowWidth) != m_P;
}

bool LedDisplayWidget::ghostOn() const
{
    return m_ghostOverride < 0 ? Theme::skin().ledShowGhost : m_ghostOverride != 0;
}

void LedDisplayWidget::overrideGhostForTest(int ghost)
{
    m_ghostOverride = ghost;
    ++m_skinGeneration;  // the atlas and the frame hold the old setting
    ensureResources();
    update();
}

void LedDisplayWidget::overrideDprForTest(qreal dpr)
{
    m_dprOverride = dpr;
}

void LedDisplayWidget::rebuildForDprForTest(qreal dpr)
{
    m_dprOverride = dpr;
    ensureResources();
    update();
}

void LedDisplayWidget::watchWindow()
{
    QWindow *w = windowHandle();
    if (!w)
        return;
    QObject::disconnect(m_screenConn);
    m_screenConn = connect(w, &QWindow::screenChanged, this, [this] {
        ensureResources();
        update();
    });
}

bool LedDisplayWidget::event(QEvent *e)
{
    const bool r = QWidget::event(e);
    if (e->type() == QEvent::ScreenChangeInternal || e->type() == QEvent::WinIdChange) {
        watchWindow();
        if (resourcesStale()) {
            ensureResources();
            update();
        }
    }
    return r;
}

void LedDisplayWidget::showEvent(QShowEvent *event)
{
    watchWindow();
    ensureResources();
    QWidget::showEvent(event);
}

void LedDisplayWidget::ensureResources()
{
    if (!resourcesStale())
        return;
    const qreal dpr = currentDpr();
    const int oldCells = m_cells;
    m_dpr = dpr;
    m_pitchBuilt = m_pitch;
    m_skinGenerationBuilt = m_skinGeneration;
    m_P = DisplayWidget::devicePitch(m_pitch, dpr, m_windowWidth);
    m_cells = Theme::skin().ledCells;
    if (m_cells != oldCells)
        m_readout = normalised(m_readout);
    if (m_readout.isEmpty())
        m_readout = QString(m_cells, QLatin1Char('-'));

    // The atlas depends on (P, colours, ghost switch): a DPR change that leaves P alone keeps it.
    const bool ghost = ghostOn();
    if (m_atlas.isNull() || m_P != m_atlasP || m_atlasSkinGeneration != m_skinGeneration || ghost != m_atlasGhost) {
        m_atlasP = m_P;
        m_atlasSkinGeneration = m_skinGeneration;
        m_atlasGhost = ghost;
        buildAtlas();
    }

    // Window and widget size in logical px (DESIGN-2.md 4.2): the widget is DisplayWidget's.
    const int F = m_cells == 2 ? 27 : 38;
    const int fieldLogW = int(std::ceil(F * double(m_P) / dpr - 1e-9));
    const int fieldLogH = int(std::ceil(19.0 * m_P / dpr - 1e-9));
    m_windowW = fieldLogW + 22;
    const int bezelW = int(std::ceil(243.0 * m_P / dpr - 1e-9)) + 38;
    const int bezelH = fieldLogH + 22;

    // Text positions: cap tops align with the digit tops, the context with the digit bottoms.
    const QFontMetricsF nameMetrics(Theme::font(Theme::displayNameRole(m_pitch)));
    m_nameBaseline = 11 + qRound(2.0 * m_P / dpr) + qRound(nameMetrics.capHeight());
    m_contextBaseline = 11 + qRound(17.0 * m_P / dpr);
    m_splitY = (m_nameBaseline + m_contextBaseline) / 2;

    const QSize newSize(bezelW, bezelH);
    buildFrame();
    const bool resized = newSize != size();
    setFixedSize(newSize);
    if (resized)
        emit sizeChanged();
}

void LedDisplayWidget::buildAtlas()
{
    ++g_atlasBuilds;
    const Theme::Palette &col = Theme::colors();
    const bool ghost = ghostOn();
    const int P = m_P;
    const int cw = 11 * P;
    const int ch = 15 * P;
    m_atlas = QImage(CharacterCount * 2 * cw, ch, QImage::Format_RGB32);
    m_atlas.fill(col.ledBack);
    QPainter p(&m_atlas);
    p.setPen(Qt::NoPen);
    p.setRenderHint(QPainter::Antialiasing, true);  // slanted edges need it (DESIGN-2.md 4.2)
    for (int c = 0; c < CharacterCount; ++c) {
        for (int dot = 0; dot < 2; ++dot) {
            const double ox = (2 * c + dot) * double(cw);
            if (ghost) {
                for (int s = 0; s < kSegmentCount; ++s) {
                    if (!(kMasks[c] & (1 << s)))
                        fillPolygon(p, kSegments[s], 6, P, ox, 0, col.ledSegGhost);
                }
                if (!dot)
                    fillPolygon(p, kDot, 4, P, ox, 0, col.ledSegGhost);
            }
            for (int s = 0; s < kSegmentCount; ++s) {
                if (kMasks[c] & (1 << s))
                    fillPolygon(p, kSegments[s], 6, P, ox, 0, col.ledSegOn);
            }
            if (dot)
                fillPolygon(p, kDot, 4, P, ox, 0, col.ledSegOn);
        }
    }
}

void LedDisplayWidget::paintCell(QPainter &p, int cell, int character)
{
    const int P = m_P;
    const int cw = 11 * P;
    p.drawImage(m_fieldX + (3 + 11 * cell) * P, m_fieldY + 2 * P, m_atlas, (2 * character) * cw, 0, cw, 15 * P);
}

// Clears the changed strip(s) to the panel colour and draws the text again (DESIGN-2.md 4.2). The
// painter is on the frame with its DPR neutral; the text is laid out in logical px under a scale.
void LedDisplayWidget::paintText(QPainter &p, bool name, bool context, const QSize &logicalSize)
{
    const Theme::Palette &col = Theme::colors();
    const int bezelW = logicalSize.width();
    const int bezelH = logicalSize.height();
    const int left = m_windowW + 24;
    const int avail = qMax(0, bezelW - left - 8);
    p.save();
    p.scale(m_dpr, m_dpr);
    if (name) {
        const QRect strip(m_windowW, 0, bezelW - m_windowW, m_splitY);
        p.setClipRect(strip);
        p.fillRect(strip, col.panel);
        if (!m_name.isEmpty()) {
            const QFont font = Theme::font(Theme::displayNameRole(m_pitch));
            p.setFont(font);
            p.setPen(col.labelPrimary);
            p.drawText(QPoint(left, m_nameBaseline), QFontMetrics(font).elidedText(m_name, Qt::ElideRight, avail));
        }
    }
    if (context) {
        const QRect strip(m_windowW, m_splitY, bezelW - m_windowW, bezelH - m_splitY);
        p.setClipRect(strip);
        p.fillRect(strip, col.panel);
        if (!m_context.isEmpty()) {
            const QFont font = Theme::font(Theme::displayContextRole(m_pitch));
            const QColor &colour = m_tone == Tone::Primary ? col.labelPrimary
                                   : m_tone == Tone::Red   ? col.labelRed
                                   : m_tone == Tone::Lit   ? col.ledSegOn
                                                           : col.labelSecondary;
            p.setFont(font);
            p.setPen(colour);
            p.drawText(QPoint(left, m_contextBaseline), QFontMetrics(font).elidedText(m_context, Qt::ElideRight, avail));
        }
    }
    p.restore();
}

// The whole widget: panel, the window with its frame, the field, the cells, the name and the
// context line. Called from ensureResources() only.
void LedDisplayWidget::buildFrame()
{
    ++m_frameBuilds;
    const Theme::Palette &col = Theme::colors();
    const qreal dpr = m_dpr;
    const int bezelW = int(std::ceil(243.0 * m_P / dpr - 1e-9)) + 38;
    const int fieldLogH = int(std::ceil(19.0 * m_P / dpr - 1e-9));
    const int bezelH = fieldLogH + 22;
    const int F = m_cells == 2 ? 27 : 38;
    const int fieldLogW = int(std::ceil(F * double(m_P) / dpr - 1e-9));
    const int pixW = qRound(bezelW * dpr);
    const int pixH = qRound(bezelH * dpr);
    const int t = qMax(1, qRound(dpr));  // one logical px
    const int winPx = qRound(m_windowW * dpr);
    m_fieldX = qRound(11 * dpr);
    m_fieldY = qRound(11 * dpr);
    const int fieldPxW = qRound(fieldLogW * dpr);
    const int fieldPxH = qRound(fieldLogH * dpr);

    m_frame = QPixmap(pixW, pixH);  // device pixels; the DPR is set after painting
    {
        QPainter p(&m_frame);
        p.fillRect(0, 0, pixW, pixH, col.panel);
        p.fillRect(0, 0, winPx, pixH, col.bezel);
        p.fillRect(0, 0, winPx, t, col.bezelRimTop);
        p.fillRect(0, t, t, pixH - t, col.bezelRim);
        p.fillRect(winPx - t, t, t, pixH - t, col.bezelRim);
        p.fillRect(t, pixH - t, winPx - 2 * t, t, col.bezelRim);
        p.fillRect(m_fieldX, m_fieldY, fieldPxW, fieldPxH, col.ledBack);
        for (int k = 0; k < m_cells; ++k)
            paintCell(p, k, characterIndex(m_readout.at(k)));
        paintText(p, true, true, QSize(bezelW, bezelH));  // width() is still the old size here
    }
    m_frame.setDevicePixelRatio(dpr);
}

void LedDisplayWidget::paintEvent(QPaintEvent *)
{
    // Paint only blits the cached frame. If the DPR moved under us (no screen-change signal
    // seen yet), rebuild from the event loop and draw the stale frame now.
    if (resourcesStale() && !m_rebuildQueued) {
        m_rebuildQueued = true;
        QMetaObject::invokeMethod(
            this,
            [this] {
                m_rebuildQueued = false;
                ensureResources();
                update();
            },
            Qt::QueuedConnection);
    }
    QPainter p(this);
    p.drawPixmap(0, 0, m_frame);
}

}  // namespace d05
