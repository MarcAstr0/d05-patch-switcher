#include "ui/Nameplate.h"

#include "ui/Theme.h"

#include <QFile>
#include <QFontMetricsF>
#include <QPainter>

namespace d05 {

namespace {
const QString &wordmarkText()
{
    static const QString s = QStringLiteral("PATCH SWITCHER");
    return s;
}
const QString &subText()
{
    static const QString s = QStringLiteral("FOR ROLAND D-05");
    return s;
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
    if (!logoPath.isEmpty() && QFile::exists(logoPath))
        m_logo.load(logoPath);  // stays null if unreadable: the wordmark shows instead
    m_wordmarkAdvance = advanceOf(Theme::Role::Wordmark, wordmarkText());
}

int Nameplate::wordmarkWidth()
{
    return advanceOf(Theme::Role::Wordmark, wordmarkText()) + 14 + 1 + 15
           + advanceOf(Theme::Role::WordmarkSub, subText());
}

QSize Nameplate::logoSize() const
{
    if (m_logo.isNull())
        return {};
    // The largest size that fits the slot and is not larger than source px / DPR.
    const qreal dpr = devicePixelRatioF();
    const QSizeF source = QSizeF(m_logo.size()) / dpr;
    const QSizeF slot(qMin(width(), SlotMaxWidth), SlotHeight);
    const qreal k = qMin(1.0, qMin(slot.width() / source.width(), slot.height() / source.height()));
    return QSize(qMax(1, qRound(source.width() * k)), qMax(1, qRound(source.height() * k)));
}

void Nameplate::rescale()
{
    const qreal dpr = devicePixelRatioF();
    const QSize logical = logoSize();
    if (dpr == m_scaledDpr && logical == m_scaledFor && !m_scaled.isNull())
        return;
    const QSize device(qRound(logical.width() * dpr), qRound(logical.height() * dpr));
    m_scaled = QPixmap::fromImage(m_logo.scaled(device, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_scaled.setDevicePixelRatio(dpr);
    m_scaledDpr = dpr;
    m_scaledFor = logical;
}

void Nameplate::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    if (hasLogo()) {
        rescale();
        const QSize s = logoSize();
        p.drawPixmap(QPoint(0, (SlotHeight - s.height()) / 2), m_scaled);
        return;
    }
    p.setFont(Theme::font(Theme::Role::Wordmark));
    p.setPen(Theme::labelAccent);
    p.drawText(QPoint(0, 20), wordmarkText());
    const int ruleX = m_wordmarkAdvance + 14;
    p.fillRect(QRect(ruleX, 9, 1, 12), Theme::ruleSecondary);
    p.setFont(Theme::font(Theme::Role::WordmarkSub));
    p.setPen(Theme::labelSecondary);
    p.drawText(QPoint(ruleX + 15, 20), subText());
}

}  // namespace d05
