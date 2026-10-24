#pragma once

#include <QDialog>
#include <QString>

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
    // The text of every paragraph, in order (tests).
    QStringList paragraphs() const;
};

}  // namespace d05
