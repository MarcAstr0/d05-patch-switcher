#pragma once

#include "app/AppSettings.h"

#include <QDialog>
#include <QString>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;

namespace d05 {

class MidiBackend;
class MidiLink;
class SectionRule;

// The Settings dialog (DESIGN.md 10.1, SPEC FR-9). Fixed 520 x 436 client area, laid out
// at the design's exact coordinates.
//
// The dialog changes nothing by itself: it edits a copy of the values and reports them
// (accepted() with values(), testRequested() for TEST). The live connection is never
// touched while the dialog is open, so the connection line is a preview built from the
// backend's port list: a port that is listed reads "Connected to <port>" (it will be
// opened when the values are applied), a port that is not reads "Port not found", a failed
// enumeration reads "Error: <text>". For the port that is already open the link's own
// status is shown, so a real open error is visible. The backend is used here only to list
// ports; nothing is opened or sent through it.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    static constexpr int ClientWidth = 520;
    static constexpr int ClientHeight = 436;

    SettingsDialog(MidiBackend &backend, const MidiLink &link, const AppSettings::Values &initial,
                   QWidget *parent = nullptr);

    // What the controls hold now. Ports are keys (no ALSA client:port numbers).
    AppSettings::Values values() const;
    // True once TEST was pressed in this session.
    bool testPressed() const { return m_testPressed; }

    // Parts, for tests and the screenshot driver.
    QComboBox *outputCombo() const { return m_outCombo; }
    QComboBox *inputCombo() const { return m_inCombo; }
    QSpinBox *channelSpin() const { return m_channel; }
    QCheckBox *followCheck() const { return m_follow; }
    QCheckBox *autoSendCheck() const { return m_autoSend; }
    QCheckBox *resendCheck() const { return m_resend; }
    QPushButton *refreshButton() const { return m_refresh; }
    QPushButton *testButton() const { return m_test; }
    QPushButton *okButton() const { return m_ok; }
    QPushButton *cancelButton() const { return m_cancel; }
    QLabel *connectionLabel() const { return m_connectionText; }
    // "ok" or "error": the connection line's state.
    QString connectionState() const;
    QString connectionText() const { return m_fullStatus; }

signals:
    // TEST: send the current patch with these (pending) values.
    void testRequested(const d05::AppSettings::Values &values);

private:
    void buildUi();
    void refreshPorts();
    void fillCombo(QComboBox *combo, const QStringList &names, const QString &wanted, bool withNone);
    void updateStatus();
    void updateEnabled();

    MidiBackend &m_backend;
    const MidiLink &m_link;
    AppSettings::Values m_initial;
    bool m_testPressed = false;
    QString m_listError;
    QStringList m_outNames;
    QString m_fullStatus;

    QComboBox *m_outCombo = nullptr;
    QComboBox *m_inCombo = nullptr;
    QSpinBox *m_channel = nullptr;
    QCheckBox *m_follow = nullptr;
    QCheckBox *m_autoSend = nullptr;
    QCheckBox *m_resend = nullptr;
    QPushButton *m_refresh = nullptr;
    QPushButton *m_test = nullptr;
    QPushButton *m_ok = nullptr;
    QPushButton *m_cancel = nullptr;
    QLabel *m_connectionText = nullptr;
    QWidget *m_led = nullptr;
};

}  // namespace d05
