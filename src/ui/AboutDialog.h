#pragma once

#include <QDialog>
#include <QString>

class QLabel;

namespace d05 {

// The About box (DESIGN.md 10.4): wordmark, version, what the app does, licence, the
// trademark notice and the typeface credit. Width 440.
class AboutDialog : public QDialog {
    Q_OBJECT
public:
    static constexpr int ClientWidth = 440;

    explicit AboutDialog(const QString &version, QWidget *parent = nullptr);

    // The non-affiliation notice (SPEC 9, README "Trademarks").
    static QString trademarkNotice();
    static QString projectUrl();
    // The project link line (DESIGN 10.4); mouse only, opens the system browser.
    QLabel *linkLabel() const { return m_link; }
    // The text of every paragraph, in order (tests).
    QStringList paragraphs() const;

private:
    QLabel *m_link = nullptr;
};

}  // namespace d05
