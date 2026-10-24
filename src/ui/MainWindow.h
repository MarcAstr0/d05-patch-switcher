#pragma once

#include "core/PatchAddress.h"
#include "ui/PatchListModel.h"

#include <QMainWindow>
#include <QModelIndex>
#include <optional>

class QAction;
class QFrame;
class QLabel;
class QLineEdit;
class QMenu;
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
// Views, favourites, rename, settings, auto-send and follow arrive in later tasks;
// their actions exist here and report that they are not available yet.
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
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class Restore { Keep, First, Current };

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

    PatchCatalog &m_catalog;
    Library &m_library;
    MidiLink &m_link;
    PatchAddress m_current;
    int m_bandTop = 0;
    Restore m_restore = Restore::Keep;
    std::optional<PatchAddress> m_savedHighlight;
    QString m_inputLabel;
    bool m_firstShow = true;

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
};

}  // namespace d05
