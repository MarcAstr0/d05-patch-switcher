#pragma once

#include "app/AppSettings.h"
#include "core/PatchAddress.h"
#include "ui/PatchListModel.h"
#include "ui/SettingsDialog.h"

#include <QMainWindow>
#include <QMessageBox>
#include <QModelIndex>
#include <QPointer>
#include <QString>
#include <optional>

class QAction;
class QFrame;
class QLabel;
class QLineEdit;
class QMenu;
class QMenuBar;
class QMessageBox;
class QButtonGroup;

namespace d05 {

class BankGrid;
class DisplayWidget;
class GroupBar;
class Library;
class MidiLink;
class Nameplate;
class PanelButton;
class PatchCatalog;
class PatchDelegate;
class PatchListView;

// The one window (DESIGN.md 4 to 9, SPEC FR-2 to FR-5, FR-12 to FR-14): display, group
// bar, bank/number grid, control row, search field, patch list, status bar, and every
// keyboard shortcut of FR-13 that is in scope. It never sends on a mere highlight:
// only Enter, a click, PREV/NEXT, PgUp/PgDn or a completed grid entry do.
//
// Settings (FR-9, FR-10, FR-11): setServices() hands the window the persistent settings and
// the backend (for port listing in the dialog); applySettings() puts a set of values into
// effect (start-up and the dialog's OK), and nothing is written to disk except by the
// user's own actions (OK, the AUTO SEND toggle, the window geometry on close).
// Auto-send sends when the keyboard moves the list highlight; follow, search, view
// switches and start-up never send.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(PatchCatalog &catalog, Library &library, MidiLink &link, QWidget *parent = nullptr);
    ~MainWindow() override;

    // The current patch: restored without sending anything (startup, screenshots).
    void setCurrent(const PatchAddress &address);
    PatchAddress current() const { return m_current; }
    // Right-aligned status bar message; stays until replaced.
    void showMessage(const QString &text);
    QString message() const;
    // The input port label shown after IN (empty = None).
    void setInputPortLabel(const QString &name);
    // Re-reads port, channel and connection from the MidiLink into the status bar and display.
    void refreshConnection();

    // `settings` is where OK, the AUTO SEND toggle, the last patch (on quit) and the window
    // geometry are saved. May be null (the Settings action then says it is not available).
    void setServices(AppSettings *settings);
    // Puts `values` into effect: channel, ports, follow, auto-send, resend-on-connect. Does not save.
    void applySettings(const AppSettings::Values &values);
    // The values in effect now (the open output as its port key).
    AppSettings::Values currentSettings() const;
    void setAutoSend(bool on);  // persists when settings are attached
    bool autoSend() const { return m_autoSendOn; }
    bool resendOnConnect() const { return m_resendOnConnect; }
    // Opens the Settings dialog (window-modal, not blocking).
    void openSettings();
    // Null once the dialog is closed and deleted.
    SettingsDialog *settingsDialog() const { return m_settingsDialog.data(); }

    // Parts, for tests and the screenshot driver.
    DisplayWidget *display() const { return m_display; }
    PatchListView *list() const { return m_list; }
    PatchListModel *model() const { return m_model; }
    QLineEdit *searchField() const { return m_search; }
    BankGrid *bankGrid() const { return m_grid; }
    GroupBar *groupBar() const { return m_groupBar; }
    PanelButton *prevButton() const { return m_prev; }
    PanelButton *nextButton() const { return m_next; }
    PanelButton *viewButton(PatchListModel::View view) const;
    PanelButton *autoSendButton() const { return m_autoSend; }
    PanelButton *systemButton() const { return m_system; }
    Nameplate *nameplate() const { return m_nameplate; }
    QMenu *systemMenu() const { return m_systemMenu; }
    // An action by object name (e.g. "actionPrev"), or nullptr.
    QAction *action(const QString &name) const;
    // The highlighted row's address, if any.
    std::optional<PatchAddress> highlighted() const;
    // Vertical band top B (DESIGN 4.3).
    int bandTop() const { return m_bandTop; }

    // Library actions; none of them sends MIDI.
    void toggleFavorite(const PatchAddress &address);
    void toggleHighlightedFavorite();
    // F2: opens the inline editor on the highlighted user slot; a preset says so in the status bar.
    void renameHighlighted();
    // Opens the DESIGN 10.5 confirmation (window-modal, not blocking); CLEAR empties the recents.
    void confirmClearRecents();
    // Null once the box is closed and deleted (deletion can lag behind closing).
    QMessageBox *clearRecentsBox() const { return m_clearBox.data(); }

    // Sends and selects the highlighted patch (Enter).
    void sendHighlighted();
    // One step in catalogue order, then send (PgUp/PgDn, PREV/NEXT).
    void step(int direction);
    // Panel-style entry from the keyboard (Ctrl+G).
    void beginGridEntry();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool event(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class Restore { Keep, First, Highlight, BeforeSearch };

    void buildWidgets();
    void buildActions();
    void layoutAll();
    bool sendAddress(const PatchAddress &address);
    void onSent(const PatchAddress &address);
    void applyCurrent(const PatchAddress &address);
    void onQueryChanged(const QString &text);
    void onViewToggled();
    void onModelAboutToReset();
    void onModelReset();
    void onEntryChanged();
    void onEscape();
    void focusSearch();
    void moveHighlight(int delta);
    void highlightRow(int row, bool scrollNearTop);
    void highlightCurrent(bool scrollNearTop);
    void notAvailable(const QString &what);
    void updateDisplayConnection();
    void relayoutStatus();
    void popupSystemMenu();
    void onNameCommitted(const PatchAddress &address, const QString &text);
    void refreshDisplayPatch();
    void onFollowed(const PatchAddress &address);
    void onConnectedChanged(bool connected);
    void onHighlightMovedByKey();
    void resendCurrent();
    void noteConnected();
    void setOutput(const QString &port);
    void onSettingsTest(const AppSettings::Values &values);
    void onSettingsAccepted(SettingsDialog *dialog);
    void onSettingsRejected();
    bool sameOutput(const QString &port) const;
    void saveStateNow();
    void noteSettingsWrite();
    bool sendQuiet(const PatchAddress &address);
    void setAutoSend(bool on, bool persist);
    // Stores and closes an open rename editor (before anything resets the list).
    void finishEditing();

    PatchCatalog &m_catalog;
    Library &m_library;
    MidiLink &m_link;
    PatchAddress m_current;
    int m_bandTop = 0;
    Restore m_restore = Restore::Keep;
    std::optional<PatchAddress> m_savedHighlight;
    std::optional<PatchAddress> m_preSearch;
    QString m_startupError;
    QString m_inputLabel;
    QString m_inputName;  // the input port as last applied (raw, for change detection)
    bool m_firstShow = true;

    AppSettings *m_settings = nullptr;
    bool m_autoSendOn = false;
    bool m_resendOnConnect = false;
    int m_searchRepeatKey = 0;  // the Up/Down key whose auto-repeat moved the search field highlight
    bool m_quietSend = false;   // the send being made is not a user selection: no Recents entry
    bool m_resendBlocked = false;  // the last resend failed: no more until a send works or settings change
    int m_connGeneration = 0;      // bumped on every connect and disconnect
    int m_testedGeneration = -1;   // m_connGeneration when TEST sent
    // Settings dialog session.
    QPointer<SettingsDialog> m_settingsDialog;
    bool m_settingsOpen = false;
    bool m_trialApplied = false;  // TEST changed the live port or channel; Cancel must undo it
    bool m_writeReported = false;  // the settings-not-saved message was shown
    bool m_connectPending = false;  // the output connected while the dialog was open
    QString m_testedTarget;         // "<port key>/<channel>" of the last TEST
    AppSettings::Values m_snapshot;

    QWidget *m_central = nullptr;
    QFrame *m_band = nullptr;
    Nameplate *m_nameplate = nullptr;
    DisplayWidget *m_display = nullptr;
    GroupBar *m_groupBar = nullptr;
    BankGrid *m_grid = nullptr;
    PanelButton *m_prev = nullptr;
    PanelButton *m_next = nullptr;
    PanelButton *m_viewAll = nullptr;
    PanelButton *m_viewFav = nullptr;
    PanelButton *m_viewRecent = nullptr;
    PanelButton *m_autoSend = nullptr;
    PanelButton *m_system = nullptr;
    QButtonGroup *m_viewGroup = nullptr;
    QLineEdit *m_search = nullptr;
    PatchListView *m_list = nullptr;
    PatchListModel *m_model = nullptr;
    PatchDelegate *m_delegate = nullptr;
    QMenu *m_systemMenu = nullptr;
    QWidget *m_statusStrip = nullptr;
    QMenuBar *m_nativeMenuBar = nullptr;  // macOS only
    QPointer<QMessageBox> m_clearBox;
    std::optional<PatchAddress> m_lastHighlight;  // the last highlight seen before a reset
};

}  // namespace d05
