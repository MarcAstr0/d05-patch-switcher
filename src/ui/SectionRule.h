#pragma once

#include <QString>
#include <QWidget>

class QPainter;

namespace d05 {

// A title set into a hairline (DESIGN.md 6.1). Height 12; the rule passes through the
// lower part of the capitals and is interrupted around the centred title. The main
// window's group bar and bank grid paint theirs with the static paint(); dialogs use
// the widget.
class SectionRule : public QWidget {
    Q_OBJECT
public:
    static constexpr int Height = 12;

    explicit SectionRule(const QString &title, bool accent = true, QWidget *parent = nullptr);

    QString title() const { return m_title; }
    bool accent() const { return m_accent; }
    QSize sizeHint() const override;

    // Draws the rule into (x, y, width, Height). Accent rules take the current skin's style
    // (hairline, doubleBar or band, DESIGN-2.md 5.1); secondary rules are always the hairline.
    // `engine` selects the ENGINE band colours of the JU-06A skins.
    static void paint(QPainter *p, int x, int y, int width, const QString &title, bool accent,
                      bool engine = false);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_title;
    bool m_accent;
};

}  // namespace d05
