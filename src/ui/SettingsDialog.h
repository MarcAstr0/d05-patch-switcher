#pragma once

#include "app/AppSettings.h"
#include "core/Identify.h"
#include "core/Model.h"
#include "ui/DevicePage.h"

#include <QDialog>
#include <QString>
#include <QStringList>

#include <array>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;

namespace d05 {

class DeviceListWidget;
class DeviceRouter;
class MidiLink;

// The Settings dialog (DESIGN-2.md 7, SPEC-2 FR-9). Fixed 760 x 610 client area: the device list
// (D-05, JP-08, JU-06A, each with an enabled checkbox) on the left, the shown device's page on the
// right, the global section (auto-send, follow switches device) and TEST, CANCEL, OK below.
//
// The dialog changes nothing by itself: it edits a copy of the values of all three devices and
// reports them (accepted() with state(), testRequested() for TEST). The live connections are never
// touched while the dialog is open, so each connection line is a preview built from the backend's
// port list (see DevicePage). Detection (docs/IDENTIFY.md) runs when the port list is refreshed,
// at construction and on REFRESH, never on a timer.
//
// While two enabled devices would share an output port and a channel (or an input port and a
// channel with follow on in both), the page of the later device names the other in place and OK is
// disabled. Ports are listed through the D-05 link's backend; nothing is opened or sent from here.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    static constexpr int ClientWidth = 760;
    static constexpr int ClientHeight = 610;

    // What the dialog edits: the three devices and the two global switches.
    struct State {
        std::array<AppSettings::DeviceValues, 3> devices;
        bool autoSend = false;
        bool followSwitches = true;
    };

    // `shown` is the device whose page is shown first. `systemLookup` false skips the Linux USB
    // serial rule of the detection (tests, the fake backend).
    SettingsDialog(DeviceRouter &router, const State &initial, Model shown, QWidget *parent = nullptr,
                   bool systemLookup = true);

    State state() const;
    // The version 1 view: the D-05 page's values and the global auto-send.
    AppSettings::Values values() const;
    AppSettings::DeviceValues deviceValues(Model m) const;

    // True once TEST was pressed in this session (for any device / for `m`).
    bool testPressed() const { return m_testPressed[0] || m_testPressed[1] || m_testPressed[2]; }
    bool testPressed(Model m) const { return m_testPressed[int(m)]; }

    // TEST was refused because the live configuration of `other` holds the port and channel: the
    // page of `m` names it in place (until the next edit recomputes the conflict text).
    void showTestRefused(Model m, Model other);

    // The device whose page is shown.
    Model shown() const { return m_shown; }
    void setShown(Model m);
    DevicePage *page(Model m) const { return m_pages[int(m)]; }
    DevicePage *currentPage() const { return m_pages[int(m_shown)]; }
    DeviceListWidget *deviceList() const { return m_list; }
    // True while any pair of enabled devices collides (OK is disabled).
    bool hasConflict() const { return m_conflict; }

    // Parts of the shown page and the dialog, for tests and the screenshot driver.
    QComboBox *outputCombo() const { return currentPage()->outputCombo(); }
    QComboBox *inputCombo() const { return currentPage()->inputCombo(); }
    QSpinBox *channelSpin() const { return currentPage()->channelSpin(); }
    QCheckBox *followCheck() const { return currentPage()->followCheck(); }
    QCheckBox *resendCheck() const { return currentPage()->resendCheck(); }
    QPushButton *refreshButton() const { return currentPage()->refreshButton(); }
    QLabel *connectionLabel() const { return currentPage()->connectionLabel(); }
    // "ok" or "error": the shown page's connection line state.
    QString connectionState() const { return currentPage()->connectionState(); }
    QString connectionText() const { return currentPage()->connectionText(); }
    // The shown page's input row problem text, empty when none.
    QString inputProblem() const { return currentPage()->inputProblem(); }
    QCheckBox *autoSendCheck() const { return m_autoSend; }
    QCheckBox *followSwitchesCheck() const { return m_followSwitches; }
    QPushButton *testButton() const { return m_test; }
    QPushButton *okButton() const { return m_ok; }
    QPushButton *cancelButton() const { return m_cancel; }

signals:
    // TEST: send the device's current patch with these (pending) values.
    void testRequested(d05::Model model, const d05::AppSettings::DeviceValues &values);

private:
    void buildUi(Model shown, const State &initial);
    void refreshPorts();
    void refreshEngineInfo();
    void updateAll();  // the list rows, the conflict texts, OK and TEST
    QString conflictTextFor(Model m, const DeviceRouter &router) const;

    DeviceRouter &m_router;
    bool m_systemLookup;
    Model m_shown = Model::D05;
    std::array<bool, 3> m_enabled{};
    std::array<bool, 3> m_testPressed{};
    bool m_conflict = false;

    std::array<DevicePage *, 3> m_pages{};
    DeviceListWidget *m_list = nullptr;
    QCheckBox *m_autoSend = nullptr;
    QCheckBox *m_followSwitches = nullptr;
    QPushButton *m_test = nullptr;
    QPushButton *m_ok = nullptr;
    QPushButton *m_cancel = nullptr;
};

}  // namespace d05
