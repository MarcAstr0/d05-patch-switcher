#pragma once

#include <QMainWindow>

namespace d05 {

// Placeholder window until the real MainWindow lands (M3).
class ScaffoldWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit ScaffoldWindow(int presetRows, QWidget *parent = nullptr);
};

} // namespace d05
