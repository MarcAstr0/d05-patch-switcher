#include "ui/ScaffoldWindow.h"

#include <QLabel>

namespace d05 {

ScaffoldWindow::ScaffoldWindow(int presetRows, QWidget *parent)
    : QMainWindow(parent)
{
    auto *label = new QLabel(
        QStringLiteral("Scaffold build. %1 preset patches embedded. See docs/PLAN.md.")
            .arg(presetRows));
    label->setMargin(24);
    setCentralWidget(label);
    setWindowTitle(QStringLiteral("D-05 Patch Switcher"));
}

} // namespace d05
