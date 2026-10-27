#pragma once

#include "app/AppSettings.h"
#include "core/Model.h"
#include "core/PatchAddress.h"
#include "midi/DeviceRouter.h"
#include "ui/PatchListModel.h"
#include "ui/SettingsDialog.h"

#include <QMainWindow>
#include <QDialog>
#include <QModelIndex>
#include <QPair>
#include <QPointer>
#include <QString>
#include <array>
#include <memory>
#include <optional>

class QAction;
class QFrame;
class QLabel;
class QLineEdit;
class QMenu;
class QMenuBar;
class QButtonGroup;

namespace d05 {

class BankGrid;
class DeviceSwitcher;
class DisplayWidget;
class GroupBar;
class LedDisplayWidget;
class Library;
class MidiLink;
class Nameplate;
class PanelButton;
class PatchCatalog;
class PatchDelegate;
class PatchListView;
class PortHub;

// The one window (DESIGN.md 4 to 9, SPEC FR-2 to FR-5, FR-12 to FR-14): display, group
// bar, bank/number grid, control row, search field, patch list, status bar, and every
// keyboard shortcut of FR-13 that is in scope. It never sends on a mere highlight:
// only Enter, a click, PREV/NEXT, PgUp/PgDn or a completed grid entry do.
//
// Devices (SPEC-2 3.4, FR-9, FR-10, FR-16, FR-20): the window talks to a DeviceRouter, which owns
// the three device links. The router's current device decides the skin and what the switcher
// marks; switching is synchronous (one event-loop turn, one repaint) and sends nothing. A switch
// the user makes (the switcher, Ctrl+Tab, Settings) goes through DeviceRouter::setCurrentDevice;
// one that follows a panel change (FR-20) arrives as currentDeviceChanged(model, true) and the
// window updates the same way.
//
// What the list, grid and display show per model is T8.3's. Until then the D-05 is the only
// device whose catalogue, list, grid, search and LCD the window drives: while another device is
// current those widgets are hidden (so nothing of the D-05 is shown under another skin) and the
// LED display shows the device's idle or connection line.
//
// Settings (FR-9, FR-10, FR-11): setServices() hands the window the persistent settings;
// applyDevice() / applySettings() put a device's values into effect (start-up and the dialog's
// OK), and nothing is written to disk except by the user's own actions (OK, the AUTO SEND
// toggle, a device switch, the window geometry on close).
// Auto-send sends when the keyboard moves the list highlight; follow, search, view
// switches and start-up never send.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // The window on a router (the application's form). The router must outlive the window.
    MainWindow(PatchCatalog &catalog, Library &library, DeviceRouter &router, QWidget *parent = nullptr);
    // The version 1 form: a D-05 link on its own private hub. The window makes a router around it
    // (the other two devices stay disabled) and enables the D-05, as version 1 always had it.
    MainWindow(PatchCatalog &catalog, Library &library, MidiLink &link, QWidget *parent = nullptr);
    ~MainWindow() override;

    DeviceRouter *router() const { return m_router; }
    // The window's current device (the router's).
    Model currentDevice() const;
    // A device switch made by the user: sets the router's current device, which clears a JU-06A arm.
    // Only an enabled device can be switched to; anything else is ignored.
    void switchDevice(Model m);
    // Ctrl+Tab / Ctrl+Shift+Tab: the next (+1) or previous (-1) enabled device, wrapping.
    void cycleDevice(int direction);
    DeviceSwitcher *switcher() const { return m_switcher; }
    // False skips the Linux USB-serial rule in Settings' detection (the fake backend, tests).
    void setSystemLookup(bool on) { m_systemLookup = on; }

    // The current patch: restored without sending anything (startup, screenshots).
    void setCurrent(const PatchAddress &address);
    PatchAddress current() const { return m_current; }
    // Right-aligned status bar message; stays until replaced.
    void showMessage(const QString &text);
    QString message() const;
    // The input port label shown after IN (empty = None) for the D-05.
    void setInputPortLabel(const QString &name);
    // Re-reads port, channel and connection of the current device into the status bar, the display
    // and the switcher.
    void refreshConnection();

    // `settings` is where OK, the AUTO SEND toggle, the last patch (on quit) and the window
    // geometry are saved. May be null (the Settings action then says it is not available).
    void setServices(AppSettings *settings);
    // False for a --fake-midi run without --config-dir: the fake ports must never reach the
    // real settings file, so OK keeps the saved output and input port (other values are saved).
    void setPersistPorts(bool persist) { m_persistPorts = persist; }
    // Puts one device's values into effect (enables or disables it in the router): channel, ports,
    // follow, resend-on-connect. Does not save. A refused set (port and channel taken by an
    // enabled device) changes nothing and says so in the status bar.
    bool applyDevice(Model m, const AppSettings::DeviceValues &values);
    // Auto-send and follow-switches-device, without saving.
    void applyGlobal(bool autoSend, bool followSwitches);
    // Version 1 form: the D-05 enabled with `values`, plus auto-send. Does not save.
    void applySettings(const AppSettings::Values &values);
    // The D-05's values in effect now (the open output as its port key) and the auto-send.
    AppSettings::Values currentSettings() const;
    void setAutoSend(bool on);  // persists when settings are attached
    bool autoSend() const { return m_autoSendOn; }
    bool resendOnConnect() const { return m_router->config(Model::D05).resendOnConnect; }
    // Opens the Settings dialog (window-modal, not blocking).
    void openSettings();
    // Null once the dialog is closed and deleted.
    SettingsDialog *settingsDialog() const { return m_settingsDialog.data(); }

    // Parts, for tests and the screenshot driver.
    DisplayWidget *display() const { return m_display; }
    LedDisplayWidget *ledDisplay() const { return m_led; }
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
    Nameplate *nameplate() const;
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
    QDialog *clearRecentsBox() const { return m_clearBox.data(); }

    // F1: opens the shortcut sheet (DESIGN 10.3), or closes it when it is open. Sends nothing.
    void toggleShortcuts();
    QDialog *shortcutsDialog() const { return m_shortcutsBox.data(); }
    // The About box (DESIGN 10.4), version from QCoreApplication::applicationVersion().
    void openAbout();
    QDialog *aboutDialog() const { return m_aboutBox.data(); }

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

    void init();
    void buildWidgets();
    void buildActions();
    void layoutAll();
    void applySkin();
    // The device switch (user or follow): skin, hidden or shown content, switcher, connection.
    void onDeviceChanged(Model model, bool byFollow);
    void applyContentVisibility();
    void updateSwitcher();
    void updateLedDisplay();
    bool d05Current() const { return currentDevice() == Model::D05; }
    // A current device that is not enabled is replaced by the first enabled one (the D-05 when
    // none is); the skin follows.
    void ensureCurrentDevice();
    const MidiLink *currentLink() const;
    QString skinIdForCurrent() const;
    bool sendAddress(const PatchAddress &address);
    void onSent(const PatchAddress &address);
    // `moveHighlight` false for a quiet send (resend, TEST): the list stays where the user is.
    void applyCurrent(const PatchAddress &address, bool moveHighlight = true);
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
    void onConnectedChanged(Model model, bool connected);
    void onHighlightMovedByKey();
    void resendCurrent(Model model);
    void noteConnected(Model model);
    // Applies a configuration to one device through the router and treats a move from one open
    // port to another as a connect (resend-on-connect). False when the router refused it.
    bool configureDevice(Model m, const DeviceRouter::DeviceConfig &cfg);
    static DeviceRouter::DeviceConfig configFor(const AppSettings::DeviceValues &v);
    void onSettingsTest(Model model, const AppSettings::DeviceValues &values);
    void onSettingsAccepted(SettingsDialog *dialog);
    void onSettingsRejected();
    void saveStateNow();
    void noteSettingsWrite();
    bool sendQuiet(Model model);
    void setAutoSend(bool on, bool persist);
    // Stores and closes an open rename editor (before anything resets the list).
    void finishEditing();
    // Applies a rename whose commit is still queued (closing, quitting).
    void flushPendingRename();
    void saveAndQuit();

    // The version 1 constructor's private hub and router; declared first so they die last.
    std::unique_ptr<PortHub> m_ownedHub;
    std::unique_ptr<DeviceRouter> m_ownedRouter;
    PatchCatalog &m_catalog;
    Library &m_library;
    DeviceRouter *m_router = nullptr;
    MidiLink &m_link;  // the D-05's link, the router's
    PatchAddress m_current;  // the D-05's current patch
    int m_bandTop = 0;
    Restore m_restore = Restore::Keep;
    std::optional<PatchAddress> m_savedHighlight;
    std::optional<PatchAddress> m_preSearch;
    QString m_startupError;
    bool m_firstShow = true;
    bool m_systemLookup = true;
    bool m_inLayout = false;  // layoutAll() is running (its own resize of the display must not recurse)

    AppSettings *m_settings = nullptr;
    bool m_persistPorts = true;
    bool m_autoSendOn = false;
    int m_searchRepeatKey = 0;  // the Up/Down key whose auto-repeat moved the search field highlight
    std::optional<QPair<PatchAddress, QString>> m_pendingRename;
    bool m_quietSend = false;   // the send being made is not a user selection: no Recents entry
    // Per device, indexed by Model.
    struct DeviceRuntime {
        bool resendBlocked = false;  // the last resend failed: no more until a send works or settings change
        int connGeneration = 0;      // bumped on every connect and disconnect
        QString inputLabel;          // the input port as the user named it (a key), for the status bar
    };
    std::array<DeviceRuntime, 3> m_rt;
    // Settings dialog session, per device.
    struct SessionDevice {
        DeviceRouter::DeviceConfig snapshot;  // the configuration in effect when the dialog opened
        bool trialApplied = false;  // TEST changed the live port or channel; Cancel must undo it
        bool connectPending = false;  // the output connected while the dialog was open
        QString testedTarget;         // "<port key>/<channel>" of the last TEST
        int testedGeneration = -1;    // connGeneration when TEST sent
    };
    std::array<SessionDevice, 3> m_sess;
    QPointer<SettingsDialog> m_settingsDialog;
    bool m_settingsOpen = false;
    bool m_writeReported = false;  // the settings-not-saved message was shown

    QWidget *m_central = nullptr;
    QFrame *m_band = nullptr;
    DeviceSwitcher *m_switcher = nullptr;
    DisplayWidget *m_display = nullptr;
    LedDisplayWidget *m_led = nullptr;
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
    QPointer<QDialog> m_clearBox;
    QPointer<QDialog> m_shortcutsBox;
    QPointer<QDialog> m_aboutBox;
    std::optional<PatchAddress> m_lastHighlight;  // the last highlight seen before a reset
};

}  // namespace d05
