#include "ui/Nameplate.h"

#include "ui/Theme.h"

#include <QFile>
#include <QImageReader>
#include <QFontMetricsF>
#include <QEvent>
#include <QPainter>

namespace d05 {

namespace {
const QString &wordmarkText()
{
    static const QString s = QStringLiteral("PATCH SWITCHER");
    return s;
}
// The sub line of the current skin: "FOR ROLAND D-05" on the d05 skin (DESIGN-2.md 2.4).
const QString &subText()
{
    return Theme::skin().wordmarkSub;
}
int advanceOf(Theme::Role role, const QString &s)
{
    return qRound(QFontMetricsF(Theme::font(role)).horizontalAdvance(s));
}
}  // namespace

Nameplate::Nameplate(const QString &logoPath, QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(SlotHeight);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    refreshSkin();
    connect(Theme::notifier(), &Theme::Notifier::skinChanged, this, [this] { refreshSkin(); });
    if (!logoPath.isEmpty() && QFile::exists(logoPath)) {
        // Read the header first: a huge or unreadable file is never decoded.
        QImageReader reader(logoPath);
        const QSize size = reader.size();
        if (size.isValid() && size.width() <= MaxLogoSide && size.height() <= MaxLogoSide) {
            m_path = logoPath;
            m_sourceSize = size;
        }
    }
    rescale();
}

// The wordmark face, its width and the sub line follow the skin (DESIGN-2.md 2.6, 3.1);
// rebuilt only on a skin change, never while painting.
void Nameplate::refreshSkin()
{
    m_wordFont = Theme::font(Theme::Role::Wordmark);
    m_subFont = Theme::font(Theme::Role::WordmarkSub);
    m_subText = m_subOverride.isEmpty() ? subText() : m_subOverride;
    m_wordmarkAdvance = advanceOf(Theme::Role::Wordmark, wordmarkText());
    update();
}

int Nameplate::wordmarkWidth(const QString &sub)
{
    return advanceOf(Theme::Role::Wordmark, wordmarkText()) + 14 + 1 + 15
           + advanceOf(Theme::Role::WordmarkSub, sub.isEmpty() ? subText() : sub);
}

void Nameplate::setSubText(const QString &sub)
{
    m_subOverride = sub;
    refreshSkin();
}

QSize Nameplate::logoSize() const
{
    if (m_sourceSize.isEmpty())
        return {};
    // The largest size that fits the slot and is not larger than source px / DPR.
    const qreal dpr = devicePixelRatioF();
    const QSizeF source = QSizeF(m_sourceSize) / dpr;
    const QSizeF slot(qMin(width(), SlotMaxWidth), SlotHeight);
    const qreal k = qMin(1.0, qMin(slot.width() / source.width(), slot.height() / source.height()));
    return QSize(qMax(1, qRound(source.width() * k)), qMax(1, qRound(source.height() * k)));
}

// Decodes straight to the target size and keeps only that pixmap. Called at construction,
// on resize and on a DPR change; never from paintEvent.
void Nameplate::rescale()
{
    if (!hasLogo())
        return;
    const qreal dpr = devicePixelRatioF();
    const QSize logical = logoSize();
    if (dpr == m_scaledDpr && logical == m_scaledFor && !m_scaled.isNull())
        return;
    QImageReader reader(m_path);
    reader.setScaledSize(QSize(qRound(logical.width() * dpr), qRound(logical.height() * dpr)));
    QImage img = reader.read();
    if (img.isNull()) {
        m_sourceSize = QSize();  // unreadable after all: wordmark
        m_scaled = QPixmap();
        return;
    }
    m_scaled = QPixmap::fromImage(img);
    m_scaled.setDevicePixelRatio(dpr);
    m_scaledDpr = dpr;
    m_scaledFor = logical;
}

void Nameplate::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    rescale();
}

bool Nameplate::event(QEvent *e)
{
    const bool r = QWidget::event(e);
    if (e->type() == QEvent::ScreenChangeInternal)
        rescale();
    return r;
}

int Nameplate::plateRight() const
{
    return hasLogo() ? logoSize().width() : m_wordmarkAdvance;
}

int Nameplate::paintPlate(QPainter &p, bool withSub)
{
    if (hasLogo()) {
        const QSize s = logoSize();
        p.drawPixmap(QPoint(0, (SlotHeight - s.height()) / 2), m_scaled);
        return s.width();
    }
    p.setFont(m_wordFont);
    p.setPen(Theme::wordmarkColour());
    p.drawText(QPoint(0, 20), wordmarkText());
    if (withSub) {
        const int ruleX = m_wordmarkAdvance + 14;
        p.fillRect(QRect(ruleX, 9, 1, 12), Theme::colors().ruleSecondary);
        p.setFont(m_subFont);
        p.setPen(Theme::colors().labelSecondary);
        p.drawText(QPoint(ruleX + 15, 20), m_subText);
    }
    return m_wordmarkAdvance;
}

void Nameplate::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    paintPlate(p, true);
}

}  // namespace d05
