#pragma once

#include "app/AppSettings.h"
#include "core/Identify.h"
#include "core/Model.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <optional>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;

namespace d05 {

class MidiLink;

// One device's page of the Settings dialog (DESIGN-2.md 7.3), 560 x 415: MIDI output (port, USE,
// REFRESH, channel, the conflict text, the connection line), MIDI input (port, USE, the input
// problem line, follow), resend-on-connect, and the note for the model; the JU-06A's page also
// shows its engine state and the two remembered programs as read-only information.
//
// The page edits a copy of the device's values and never touches the live connection. The
// connection line is a preview built from the port list it was given and from the link's own
// status (the rules of version 1's dialog): a listed port reads "Connected to <port>" (it will be
// opened when the values are applied), a port that is not listed reads "Port not found", a failed
// listing reads "Error: <text>". Ports the lists attribute to this page's model (docs/IDENTIFY.md)
// read "<port> . detected" in the combo; USE selects the one detected port.
class DevicePage : public QWidget {
    Q_OBJECT
public:
    static constexpr int Width = 560;
    static constexpr int Height = 415;

    // The JU-06A's engine state as the app knows it (read-only information).
    struct EngineInfo {
        int engine = 60;
        QString on60;   // "4-5" or empty (unknown)
        QString name60;
        QString on106;
        QString name106;
    };

    // What the page's connection line says, for the device list's short text.
    enum class Kind { Connected, NotFound, Error, NoPort };

    DevicePage(Model model, const MidiLink &link, QWidget *parent = nullptr);

    Model model() const { return m_model; }

    // The values the page starts from; `enabled` is the dialog's business and is not kept here.
    void setValues(const AppSettings::DeviceValues &v);
    // What the controls hold now (ports as keys, the OS name kept for ports whose key is ambiguous).
    // `enabled` is false: the dialog fills it in.
    AppSettings::DeviceValues values() const;

    // The OS's port lists with what Identify made of each name. Re-fills both combos, keeping the
    // selection (or keeping a configured port that is not listed as a "missing" entry).
    void setPorts(const QStringList &outs, const QString &outError, const QList<Identify::Result> &outIds,
                  const QStringList &ins, const QString &inError, const QList<Identify::Result> &inIds);

    void setEngineInfo(const EngineInfo &info);
    // The conflict named in place ("Channel 4 on Hapax MIDI 1 is used by the D-05."); empty clears.
    void setConflictText(const QString &text);
    QString conflictText() const { return m_conflictFull; }

    // Parts, for tests and the dialog.
    QComboBox *outputCombo() const { return m_outCombo; }
    QComboBox *inputCombo() const { return m_inCombo; }
    QSpinBox *channelSpin() const { return m_channel; }
    QCheckBox *followCheck() const { return m_follow; }
    QCheckBox *resendCheck() const { return m_resend; }
    QPushButton *refreshButton() const { return m_refresh; }
    QPushButton *useOutputButton() const { return m_useOut; }
    QPushButton *useInputButton() const { return m_useIn; }
    QLabel *connectionLabel() const { return m_connectionText; }
    QLabel *conflictLabel() const { return m_conflictLabel; }

    Kind kind() const { return m_kind; }
    // "ok" or "error": the connection line's state.
    QString connectionState() const;
    QString connectionText() const { return m_fullStatus; }
    bool connectionOk() const { return m_ok; }
    // The input row's problem text ("Port not found", "Error: ..."), empty when none.
    QString inputProblem() const { return m_inputFullStatus; }
    // The names of the detected ports in the output / input list, for tests.
    QStringList detectedOutputs() const { return m_detectedOut; }
    QStringList detectedInputs() const { return m_detectedIn; }

signals:
    void changed();             // an edit, or a status that the device list shows may have changed
    void refreshRequested();    // REFRESH

private:
    void buildUi();
    void fillCombo(QComboBox *combo, const QStringList &names, const QList<Identify::Result> &ids,
                   const QString &wanted, bool withNone);
    void updateStatus();
    void updateInputStatus();
    void updateEnabled();
    void updateUseButtons();

    Model m_model;
    const MidiLink &m_link;
    AppSettings::DeviceValues m_initial;
    QString m_listError;
    QStringList m_outNames;
    QStringList m_inNames;
    QString m_inListError;
    QString m_inputFullStatus;
    QString m_fullStatus;
    QString m_conflictFull;
    QStringList m_detectedOut;
    QStringList m_detectedIn;
    Kind m_kind = Kind::NotFound;
    bool m_ok = false;

    QComboBox *m_outCombo = nullptr;
    QComboBox *m_inCombo = nullptr;
    QSpinBox *m_channel = nullptr;
    QCheckBox *m_follow = nullptr;
    QCheckBox *m_resend = nullptr;
    QPushButton *m_refresh = nullptr;
    QPushButton *m_useOut = nullptr;
    QPushButton *m_useIn = nullptr;
    QLabel *m_connectionText = nullptr;
    QLabel *m_inputStatus = nullptr;
    QLabel *m_conflictLabel = nullptr;
    QWidget *m_led = nullptr;
    // JU-06A engine rows.
    QLabel *m_believed = nullptr;
    QLabel *m_on60 = nullptr;
    QLabel *m_on60Name = nullptr;
    QLabel *m_on106 = nullptr;
    QLabel *m_on106Name = nullptr;
};

}  // namespace d05
