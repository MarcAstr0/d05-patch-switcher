#include "ui/DisplayWidget.h"

#include "ui/GlyphTable.h"
#include "ui/Theme.h"

#include <QPaintEvent>
#include <QPainter>
#include <QWindow>
#include <QtMath>

#include <atomic>
#include <cmath>

namespace d05 {

namespace {

std::atomic<int> g_atlasBuilds{0};

constexpr int kMarginX = 19;  // bezel edge to lit field, logical px (DESIGN 5.1)
constexpr int kMarginY = 11;

const char *const kGroupNames[PatchAddress::GroupCount] = {
    "USER 1", "USER 2", "USER 3", "USER 4", "USER 5", "USER 6", "USER 7", "USER 8",
    "PRESET 1: ORIGINAL D-50", "PRESET 2: NEW FOR D-05", "PRESET 3: PN-D50-01",
    "PRESET 4: PN-D50-02", "PRESET 5: PN-D50-03", "PRESET 6: PN-D50-04",
};

inline int glyphIndex(ushort ch)
{
    return (ch >= 0x20 && ch <= 0x7E) ? int(ch) - 0x20 : int('?') - 0x20;
}

}  // namespace

DisplayWidget::DisplayWidget(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_OpaquePaintEvent);
    m_sentTimer.setTimerType(Qt::PreciseTimer);  // the coarse default may fire up to 5% early
    m_sentTimer.setSingleShot(true);
    m_sentTimer.setInterval(SentFlashMs);
    connect(&m_sentTimer, &QTimer::timeout, this, [this] {
        m_sent = false;
        m_activity = false;
        recompute();
        emit sentChanged(false);
    });
    m_text[0] = QString(Columns, QLatin1Char(' '));
    m_text[1] = m_text[0];
    ensureResources();
    recompute();
}

int DisplayWidget::atlasBuildCount()
{
    return g_atlasBuilds.load();
}

void DisplayWidget::setStep(int p, int windowWidth)
{
    p = qBound(3, p, 5);
    if (p == m_pitch && windowWidth == m_windowWidth)
        return;
    m_pitch = p;
    m_windowWidth = windowWidth;
    if (resourcesStale()) {
        ensureResources();
        update();
    }
}

int DisplayWidget::pitchForWidth(int w)
{
    return w < 1042 ? 3 : (w < 1285 ? 4 : 5);
}

void DisplayWidget::setPitch(int p)
{
    p = qBound(3, p, 5);
    if (p == m_pitch)
        return;
    m_pitch = p;
    ensureResources();
    update();
}

void DisplayWidget::setLcdShowGhost(bool on)
{
    if (on == m_showGhost)
        return;
    m_showGhost = on;
    ensureResources();
    update();
}

void DisplayWidget::setKeyModeOnLine2(bool on)
{
    if (on == m_keyModeOnLine2)
        return;
    m_keyModeOnLine2 = on;
    recompute();
}

void DisplayWidget::setDisplayEntryEcho(bool on)
{
    if (on == m_entryEcho)
        return;
    m_entryEcho = on;
    recompute();
}

void DisplayWidget::setPatch(const PatchAddress &address, const QString &name, QChar mode)
{
    if (address == m_address && name == m_name && mode == m_mode)
        return;
    m_address = address;
    m_name = name;
    m_mode = mode;
    recompute();
}

void DisplayWidget::setFirstRunHint(const QString &text)
{
    if (text == m_firstRunHint)
        return;
    m_firstRunHint = text;
    if (!m_connected)
        recompute();
}

void DisplayWidget::setConnected(bool connected)
{
    if (connected == m_connected)
        return;
    m_connected = connected;
    if (!connected && m_sent) {  // a flash makes no sense without an output
        m_sentTimer.stop();
        m_sent = false;
        m_activity = false;
        recompute();
        emit sentChanged(false);
        return;
    }
    recompute();
}

void DisplayWidget::setSearch(bool active, int matchCount)
{
    if (!active)
        matchCount = 0;
    if (active == m_searchActive && matchCount == m_matchCount)
        return;
    m_searchActive = active;
    m_matchCount = matchCount;
    recompute();
}

void DisplayWidget::setEntry(const QString &text)
{
    if (text == m_entry)
        return;
    m_entry = text;
    recompute();
}

void DisplayWidget::flashActivity()
{
    // A followed patch is not a send: it ends a SENT flash at once (line 2 goes back to the
    // group name) while the activity LED stays lit, for 400 ms from this message.
    const bool wasOn = m_sent || m_activity;
    const bool hadSent = m_sent;
    m_sent = false;
    m_activity = true;
    m_sentTimer.start();
    if (hadSent)
        recompute();
    if (!wasOn)
        emit sentChanged(true);
}

void DisplayWidget::flashSent()
{
    if (!m_connected)
        return;
    const bool wasOn = m_sent || m_activity;
    m_sent = true;
    m_sentTimer.start();  // restarts a running timer
    recompute();
    if (!wasOn)
        emit sentChanged(true);
}

QString DisplayWidget::groupLongName(int group)
{
    if (group < 0 || group >= PatchAddress::GroupCount)
        return QString();
    return QString::fromLatin1(kGroupNames[group]);
}

QString DisplayWidget::modeText(QChar mode)
{
    switch (mode.unicode()) {
    case 'W': return QStringLiteral("WHOLE");
    case 'D': return QStringLiteral("DUAL");
    case 'S': return QStringLiteral("SPLIT");
    default: return QString();
    }
}

QString DisplayWidget::normalised(const QString &s) const
{
    QString r = s.left(Columns);
    if (r.size() < Columns)
        r.append(QString(Columns - r.size(), QLatin1Char(' ')));
    return r;
}

void DisplayWidget::recompute()
{
    const QString mode = modeText(m_mode);

    // Line 1: current patch, or the entry echo alone.
    QString l1;
    if (m_entryEcho && !m_entry.isEmpty()) {
        l1 = m_entry.left(5);
    } else {
        l1 = m_address.toString() + QLatin1Char(':') + m_name.left(20);
        if (!m_keyModeOnLine2 && !mode.isEmpty()) {
            l1 = l1.leftJustified(Columns - mode.size(), QLatin1Char(' '), true);
            l1 += mode;
        }
    }

    // Line 2: FR-12 priority.
    QString l2;
    bool groupName = false;
    if (!m_connected) {
        l2 = m_firstRunHint.isEmpty() ? QStringLiteral("NO MIDI OUTPUT") : m_firstRunHint;
    } else if (m_sent) {
        l2 = QStringLiteral("SENT");
    } else if (m_searchActive) {
        l2 = m_matchCount == 0 ? QStringLiteral("NO MATCHES")
             : m_matchCount == 1 ? QStringLiteral("1 MATCH")
                                 : QStringLiteral("%1 MATCHES").arg(m_matchCount);
    } else {
        l2 = groupLongName(m_address.group());
        groupName = true;
    }
    if (groupName && m_keyModeOnLine2 && !mode.isEmpty()) {
        l2 = l2.leftJustified(Columns - mode.size(), QLatin1Char(' '), true);
        l2 += mode;
    }
    setLines(l1, l2);
}

void DisplayWidget::setLines(const QString &line1, const QString &line2)
{
    const QString n1 = normalised(line1);
    const QString n2 = normalised(line2);
    if (n1 == m_text[0] && n2 == m_text[1])
        return;
    const QString *next[Lines] = {&n1, &n2};
    ensureResources();
    // Cells are placed in device pixels: paint on the pixmap with its DPR neutral.
    m_frame.setDevicePixelRatio(1.0);
    QPainter p(&m_frame);
    for (int l = 0; l < Lines; ++l) {
        for (int c = 0; c < Columns; ++c) {
            if (next[l]->at(c) != m_text[l].at(c))
                paintCell(p, l, c, next[l]->at(c).unicode());
        }
    }
    p.end();
    m_frame.setDevicePixelRatio(m_dpr);
    m_text[0] = n1;
    m_text[1] = n2;
    update();
}

void DisplayWidget::paintCell(QPainter &p, int line, int col, ushort ch)
{
    const int P = m_P;
    const int cw = 5 * P;
    const int ch7 = 7 * P;
    p.drawImage(m_dotOffX + 6 * P * col, m_dotOffY + 8 * P * line, m_atlas, glyphIndex(ch) * cw, 0, cw, ch7);
}

qreal DisplayWidget::currentDpr() const
{
    return m_dprOverride > 0 ? m_dprOverride : devicePixelRatioF();
}

bool DisplayWidget::resourcesStale() const
{
    // A window width matters only through the pitch it yields (DESIGN 5.1): dragging the
    // window edge rebuilds nothing while P stays the same.
    return m_P == 0 || !qFuzzyCompare(currentDpr(), m_dpr) || m_pitchBuilt != m_pitch
           || m_builtGhost != m_showGhost || devicePitch(m_pitch, currentDpr(), m_windowWidth) != m_P;
}

namespace {
int bezelWidthFor(int P, qreal dpr)
{
    return int(std::ceil(243.0 * P / dpr - 1e-9)) + 38;
}
}  // namespace

// DESIGN 5.1 "Choosing P": the integer pitch whose bezel is nearest the design width
// T = 243 p + 38, taken only when the window is wide enough to hold it.
int DisplayWidget::devicePitch(int p, qreal dpr, int windowWidth)
{
    const int pa = qMax(1, int(std::floor(p * dpr + 1e-9)));
    if (p * dpr - pa < 1e-9)
        return pa;  // integer DPR step: exact
    const int pb = pa + 1;
    const int T = 243 * p + 38;
    const int wa = bezelWidthFor(pa, dpr);
    const int wb = bezelWidthFor(pb, dpr);
    if (wb + 32 <= windowWidth && qAbs(wb - T) < qAbs(wa - T))
        return pb;
    return pa;
}

int DisplayWidget::deviceGap(int P, qreal dpr)
{
    return qMax(1, qMin(int(qRound(dpr)), P / 3));
}

void DisplayWidget::overrideDprForTest(qreal dpr)
{
    m_dprOverride = dpr;
}

void DisplayWidget::rebuildForDprForTest(qreal dpr)
{
    m_dprOverride = dpr;
    ensureResources();
    update();
}

void DisplayWidget::watchWindow()
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

bool DisplayWidget::event(QEvent *e)
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

void DisplayWidget::ensureResources()
{
    if (!resourcesStale())
        return;
    const qreal dpr = currentDpr();

    m_dpr = dpr;
    m_pitchBuilt = m_pitch;
    const bool ghostChanged = m_builtGhost != m_showGhost;
    m_builtGhost = m_showGhost;
    m_P = devicePitch(m_pitch, dpr, m_windowWidth);
    m_g = deviceGap(m_P, dpr);

    // Lit field: dot area plus 2P padding all round, rounded up to whole logical px.
    const int fieldDevW = 243 * m_P;
    const int fieldDevH = 19 * m_P;
    const int fieldLogW = int(std::ceil(fieldDevW / dpr - 1e-9));
    const int fieldLogH = int(std::ceil(fieldDevH / dpr - 1e-9));
    const int pixW = int(qRound(fieldLogW * dpr));
    const int pixH = int(qRound(fieldLogH * dpr));
    m_dotOffX = (pixW - 239 * m_P) / 2;
    m_dotOffY = (pixH - 15 * m_P) / 2;

    // The atlas depends only on (P, g, ghost): a DPR change that leaves them alone keeps it.
    if (m_atlas.isNull() || m_P != m_atlasP || m_g != m_atlasG || ghostChanged) {
        m_atlasP = m_P;
        m_atlasG = m_g;
        buildAtlas();
    }

    ++m_frameBuilds;
    m_frame = QPixmap(pixW, pixH);  // device pixels; the DPR is set after painting
    m_frame.fill(Theme::colors().lcdBack);
    {
        QPainter p(&m_frame);
        for (int l = 0; l < Lines; ++l)
            for (int c = 0; c < Columns; ++c)
                paintCell(p, l, c, m_text[l].isEmpty() ? ushort(' ') : m_text[l].at(c).unicode());
    }
    m_frame.setDevicePixelRatio(dpr);

    const QSize newSize(fieldLogW + 2 * kMarginX, fieldLogH + 2 * kMarginY);
    const bool resized = newSize != size();
    setFixedSize(newSize);
    if (resized)
        emit sizeChanged();
}

void DisplayWidget::buildAtlas()
{
    ++g_atlasBuilds;
    const int P = m_P;
    const int d = qMax(1, P - m_g);
    m_atlas = QImage(kGlyphCount * 5 * P, 7 * P, QImage::Format_RGB32);
    m_atlas.fill(Theme::colors().lcdBack);
    QPainter p(&m_atlas);
    p.setPen(Qt::NoPen);
    for (int g = 0; g < kGlyphCount; ++g) {
        for (int r = 0; r < kGlyphRows; ++r) {
            for (int c = 0; c < 5; ++c) {
                const bool on = (kGlyphs[g][r] >> (4 - c)) & 1;
                if (!on && !m_showGhost)
                    continue;
                p.fillRect(g * 5 * P + c * P, r * P, d, d, on ? Theme::colors().lcdOn : Theme::colors().lcdGhost);
            }
        }
    }
}

void DisplayWidget::showEvent(QShowEvent *event)
{
    watchWindow();
    ensureResources();
    QWidget::showEvent(event);
}

void DisplayWidget::paintEvent(QPaintEvent *)
{
    // Paint only blits cached resources. If the DPR moved under us (no screen-change
    // signal seen yet), rebuild from the event loop and draw the stale frame now.
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
    const int w = width();
    const int h = height();
    p.fillRect(0, 0, w, h, Theme::colors().bezel);
    p.fillRect(0, 0, w, 1, Theme::colors().bezelRimTop);
    p.fillRect(0, 1, 1, h - 1, Theme::colors().bezelRim);
    p.fillRect(w - 1, 1, 1, h - 1, Theme::colors().bezelRim);
    p.fillRect(1, h - 1, w - 2, 1, Theme::colors().bezelRim);
    p.drawPixmap(kMarginX, kMarginY, m_frame);
}

}  // namespace d05
