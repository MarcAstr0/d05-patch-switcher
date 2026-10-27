#include "ui/MainWindow.h"

#include "app/Paths.h"
#include "core/Library.h"
#include "core/PatchCatalog.h"
#include "core/SearchQuery.h"
#include "midi/MidiBackend.h"
#include "midi/MidiLink.h"
#include "midi/PortHub.h"
#include "ui/AboutDialog.h"
#include "ui/BankGrid.h"
#include "ui/DeviceSwitcher.h"
#include "ui/DisplayWidget.h"
#include "ui/EngineControl.h"
#include "ui/GroupBar.h"
#include "ui/LedDisplayWidget.h"
#include "ui/Nameplate.h"
#include "ui/PanelButton.h"
#include "ui/PatchDelegate.h"
#include "ui/PatchListView.h"
#include "ui/Shortcuts.h"
#include "ui/ShortcutsDialog.h"
#include "ui/Theme.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QPainter>
#include <QRegularExpression>
#include <QShortcut>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QStyle>

namespace d05 {

namespace {

constexpr int kMargin = 16;
constexpr int kStatusHeight = 22;
constexpr int kColumnX = 434;

// The lower band. A QFrame so the QSS background and top edge apply; it also paints
// the panel labels above the control buttons (DESIGN 4.3, 6.5), which are text, not
// widgets, so their baselines land exactly on the grid.
class Band : public QFrame {
public:
    struct Label {
        QString text;
        int centreX;
        int baseline;  // band-local
        QColor colour;
        int left = 0;  // filled in by setLabels
    };

    explicit Band(QWidget *parent)
        : QFrame(parent)
    {
        setObjectName(QStringLiteral("band"));
        m_font = Theme::font(Theme::Role::PanelLabel);
        m_metrics = QFontMetricsF(m_font);
    }

    void setLabels(const QVector<Label> &labels)
    {
        m_labels = labels;
        for (Label &l : m_labels)
            l.left = qRound(l.centreX - m_metrics.horizontalAdvance(l.text) / 2);
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QFrame::paintEvent(event);
        QPainter p(this);
        p.setFont(m_font);
        for (const Label &l : m_labels) {
            p.setPen(l.colour);
            p.drawText(QPoint(l.left, l.baseline), l.text);
        }
    }

private:
    QFont m_font;
    QFontMetricsF m_metrics{QFont()};
    QVector<Label> m_labels;
};

// The 8 x 4 activity LED at the left of the status bar (DESIGN 8.1).
class ActivityLed : public QWidget {
public:
    explicit ActivityLed(QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("activityLed"));
        setFixedSize(8, 4);
    }
    void setLit(bool on)
    {
        if (on != m_lit) {
            m_lit = on;
            update();
        }
    }
    bool lit() const { return m_lit; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), m_lit ? Theme::colors().ledOn : Theme::colors().ledOff);
    }

private:
    bool m_lit = false;
};

// The status bar's content: positions its items by the gaps in DESIGN 8.1.
class StatusStrip : public QWidget {
public:
    explicit StatusStrip(QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("statusStrip"));
        led = new ActivityLed(this);
        out = makeLabel(QStringLiteral("OUT"), "statusLabel", Theme::Role::StatusLabel);
        port = makeLabel(QString(), "statusValue", Theme::Role::StatusValue);
        notFound = makeLabel(QStringLiteral("NOT FOUND"), "statusError", Theme::Role::StatusLabel);
        ch = makeLabel(QStringLiteral("CH"), "statusLabel", Theme::Role::StatusLabel);
        chValue = makeLabel(QStringLiteral("1"), "statusValue", Theme::Role::StatusValue);
        in = makeLabel(QStringLiteral("IN"), "statusLabel", Theme::Role::StatusLabel);
        inValue = makeLabel(QString(), "statusValue", Theme::Role::StatusValue);
        inNotFound = makeLabel(QStringLiteral("NOT FOUND"), "statusError", Theme::Role::StatusLabel);
        follow = makeLabel(QStringLiteral("FOLLOW"), "statusFollow", Theme::Role::StatusLabel);
        message = makeLabel(QString(), "statusMessage", Theme::Role::StatusValue);
        notFound->hide();
        inNotFound->hide();
        follow->hide();
    }

    // Sets a label's text and role (value or dim) in one go.
    static void setValue(QLabel *l, const QString &text, bool dim)
    {
        const char *role = dim ? "statusDim" : "statusValue";
        if (l->property("role").toString() != QLatin1String(role)) {
            l->setProperty("role", QString::fromLatin1(role));
            l->style()->unpolish(l);
            l->style()->polish(l);
        }
        l->setText(text);
    }

    void relayout()
    {
        int x = kMargin;
        led->move(x, 8);  // bar y 9 of 22, below the 1 px edge: strip y 8
        x = 30;
        // Gaps run from the end of the previous text's advance (DESIGN 8.1), rounded the
        // way the mockup does; the label itself is wide enough never to clip.
        auto place = [&](QLabel *l, int gapAfter) {
            const int adv = qRound(QFontMetricsF(l->font()).horizontalAdvance(l->text()));
            // Every baseline sits on the bar's y + 15, which is strip y 14.
            const QFontMetrics fm(l->font());
            l->setGeometry(x, 14 - fm.ascent(), qMax(adv, l->sizeHint().width()), fm.height());
            x += adv + gapAfter;
        };
        place(out, 6);
        place(port, notFound->isHidden() ? 16 : 6);
        if (!notFound->isHidden())
            place(notFound, 16);
        place(ch, 6);
        place(chValue, 16);
        place(in, 6);
        place(inValue, !inNotFound->isHidden() ? 6 : (follow->isHidden() ? 0 : 16));
        if (!inNotFound->isHidden())
            place(inNotFound, follow->isHidden() ? 0 : 16);
        if (!follow->isHidden())
            place(follow, 0);
        const int right = width() - kMargin;
        const int avail = qMax(0, right - x - 16);
        const QFontMetrics fm(Theme::font(Theme::Role::StatusValue));
        const QString shown = fm.elidedText(fullMessage, Qt::ElideRight, avail);
        message->setText(shown);
        const int mw = qRound(QFontMetricsF(message->font()).horizontalAdvance(shown));
        const int mwide = qMax(mw, message->sizeHint().width());
        const QFontMetrics mfm(message->font());
        message->setGeometry(right - mwide, 14 - mfm.ascent(), mwide, mfm.height());
    }

    QLabel *makeLabel(const QString &text, const char *role, Theme::Role font)
    {
        auto *l = new QLabel(text, this);
        l->setProperty("role", QString::fromLatin1(role));
        l->setFont(Theme::font(font));
        l->setTextFormat(Qt::PlainText);
        l->setAttribute(Qt::WA_TransparentForMouseEvents);
        l->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        return l;
    }

    ActivityLed *led;
    QLabel *out, *port, *notFound, *ch, *chValue, *in, *inValue, *inNotFound, *follow, *message;
    QString fullMessage;

protected:
    void resizeEvent(QResizeEvent *) override { relayout(); }
};

QString shortcutText(const QKeySequence &seq)
{
    return seq.toString(QKeySequence::NativeText);
}

}  // namespace

MainWindow::MainWindow(PatchCatalog &catalog, Library &library, DeviceRouter &router, QWidget *parent)
    : QMainWindow(parent)
    , m_catalog(catalog)
    , m_library(library)
    , m_router(&router)
    , m_link(*router.link(Model::D05))
{
    init();
}

// Version 1's single-link form. The hub never makes a backend: the other two devices stay disabled
// and nothing asks it for a port; the D-05 link keeps the private hub it was built with.
MainWindow::MainWindow(PatchCatalog &catalog, Library &library, MidiLink &link, QWidget *parent)
    : QMainWindow(parent)
    , m_ownedHub(new PortHub(PortHub::BackendFactory([]() -> MidiBackend * { return nullptr; })))
    , m_ownedRouter(new DeviceRouter(m_ownedHub.get(), nullptr, &link))
    , m_catalog(catalog)
    , m_library(library)
    , m_router(m_ownedRouter.get())
    , m_link(link)
{
    init();
}

void MainWindow::init()
{
    setWindowTitle(QStringLiteral("D-05 Patch Switcher"));
    {
        QIcon icon;
        for (const int sz : {16, 24, 32, 48, 64, 128})
            icon.addFile(QStringLiteral(":/icons/d05-patch-switcher-%1.png").arg(sz), QSize(sz, sz));
        icon.addFile(QStringLiteral(":/icons/d05-patch-switcher.png"), QSize(256, 256));
        setWindowIcon(icon);
    }
    setMinimumSize(800, 480);
    if (m_link.status().startsWith(QStringLiteral("Error")))
        m_startupError = m_link.status();  // the port was opened before the window existed
    if (m_link.current())
        m_current = *m_link.current();
    else
        m_current = *PatchAddress::fromParts(8, 1, 1);  // P1-11
    if (const MidiLink *jp = m_router->link(Model::JP08); jp->current())
        m_jpCurrent = *jp->current();
    else
        m_jpCurrent = *PatchAddress::fromBankNumber(Model::JP08, 1, 1);  // 1-1
    // The library makes (and keeps) the catalogues of the other two models, so that user names of
    // every device are saved with it.
    m_catalogs[int(Model::D05)] = &m_catalog;
    m_catalogs[int(Model::JP08)] = &m_library.catalog(Model::JP08);
    m_catalogs[int(Model::JU06A)] = &m_library.catalog(Model::JU06A);
    m_shown = currentDevice();
    for (Model m : allModels())  // the status bar's IN label of a device the router already configured
        m_rt[int(m)].inputLabel = MidiLink::portKey(m_router->config(m).inputPort);

    buildWidgets();
    buildActions();
    resize(960, 560);
    applySkin();  // also lays out

    // The D-05's LCD and group bar are kept current whether or not they are on screen.
    m_display->setPatch(m_current, m_catalog.name(m_current), m_catalog.mode(m_current));
    m_groupBar->setLitGroup(m_current.group());
    showCurrentPatch(false);
    syncEngineState();
    updateDisplayConnection();
    refreshConnection();
    if (!m_startupError.isEmpty())
        showMessage(m_startupError);
    onModelReset();
    highlightCurrent(true);
    m_search->setFocus();
}

// What the app believes each device shows now: the D-05's and JP-08's patch lives here (fed by the
// links), the JU-06A's in its engine tracker (SPEC-2 6); none is "no patch" on that engine.
std::optional<PatchAddress> MainWindow::currentOf(Model m) const
{
    switch (m) {
    case Model::D05: return m_current;
    case Model::JP08: return m_jpCurrent;
    case Model::JU06A: return m_router->ju06aTracker()->current();
    }
    return std::nullopt;
}

Model MainWindow::currentDevice() const
{
    return m_router->currentDevice();
}

const MidiLink *MainWindow::currentLink() const
{
    return m_router->link(m_router->currentDevice());
}

Nameplate *MainWindow::nameplate() const
{
    return m_switcher;
}

MainWindow::~MainWindow()
{
    if (!m_firstShow)  // shown at least once: remember where it was (Ctrl+Q skips closeEvent on some platforms)
        saveStateNow();
}

void MainWindow::buildWidgets()
{
    m_central = new QWidget(this);
    setCentralWidget(m_central);

    m_band = new Band(m_central);

    m_switcher = new DeviceSwitcher(Paths::logoPath(), m_central);
    m_display = new DisplayWidget(m_central);
    m_led = new LedDisplayWidget(m_central);
    m_led->hide();  // shown instead of the LCD on the JP-08 and JU-06A skins (applySkin)
    m_groupBar = new GroupBar(m_central);
    m_engine = new EngineControl(m_central);
    m_engine->hide();  // the JU-06A skins' group slot (applySkin)
    m_grid = new BankGrid(m_central);

    m_prev = new PanelButton(PanelButton::Kind::WedgeLeft, m_central);
    m_prev->setObjectName(QStringLiteral("prevButton"));
    m_next = new PanelButton(PanelButton::Kind::WedgeRight, m_central);
    m_next->setObjectName(QStringLiteral("nextButton"));

    m_viewAll = new PanelButton(PanelButton::Kind::Led, m_central);
    m_viewAll->setObjectName(QStringLiteral("viewAllButton"));
    m_viewFav = new PanelButton(PanelButton::Kind::Led, m_central);
    m_viewFav->setObjectName(QStringLiteral("viewFavouritesButton"));
    m_viewRecent = new PanelButton(PanelButton::Kind::Led, m_central);
    m_viewRecent->setObjectName(QStringLiteral("viewRecentsButton"));
    m_viewGroup = new QButtonGroup(this);
    m_viewGroup->setExclusive(true);
    for (PanelButton *b : {m_viewAll, m_viewFav, m_viewRecent}) {
        b->setCheckable(true);
        m_viewGroup->addButton(b);
    }
    m_viewAll->setChecked(true);

    m_autoSend = new PanelButton(PanelButton::Kind::Led, m_central);
    m_autoSend->setObjectName(QStringLiteral("autoSendButton"));
    m_autoSend->setCheckable(true);  // the LED follows the checked state (DESIGN 6.5)
    m_system = new PanelButton(PanelButton::Kind::Plain, m_central);
    m_system->setObjectName(QStringLiteral("systemButton"));
    // Face sets by role (DESIGN-2.md 5.2); on the D-05 skin they are the version 1 button colours.
    for (PanelButton *b : {m_viewAll, m_viewFav, m_viewRecent})
        b->setFaceSet(PanelButton::FaceSet::View);
    m_autoSend->setFaceSet(PanelButton::FaceSet::Commit);
    for (PanelButton *b : {m_prev, m_next, m_system})
        b->setFaceSet(PanelButton::FaceSet::Transport);

    m_search = new QLineEdit(m_central);
    m_search->setObjectName(QStringLiteral("searchField"));
    m_search->setPlaceholderText(QStringLiteral("Search names or addresses: brass, P3-45"));
    m_search->setClearButtonEnabled(false);
    m_search->setFont(Theme::font(Theme::Role::Field));
    m_search->setAttribute(Qt::WA_MacShowFocusRect, false);
    m_search->installEventFilter(this);

    m_model = new PatchListModel(*m_catalogs[int(m_shown)], m_library, this);
    m_list = new PatchListView(m_central);
    m_delegate = new PatchDelegate(m_list);
    m_list->setItemDelegate(m_delegate);  // before the model: the list measures rows for it
    m_list->setModel(m_model);

    // Tooltips name the action and its key (DESIGN 6.2).
    m_prev->setToolTip(QStringLiteral("Previous patch (%1)").arg(shortcutText(QKeySequence(Qt::Key_PageUp))));
    m_next->setToolTip(QStringLiteral("Next patch (%1)").arg(shortcutText(QKeySequence(Qt::Key_PageDown))));
    m_viewAll->setToolTip(QStringLiteral("All patches (%1)").arg(shortcutText(QKeySequence(Qt::CTRL | Qt::Key_1))));
    m_viewFav->setToolTip(QStringLiteral("Favourites (%1)").arg(shortcutText(QKeySequence(Qt::CTRL | Qt::Key_2))));
    m_viewRecent->setToolTip(QStringLiteral("Recently sent (%1)").arg(shortcutText(QKeySequence(Qt::CTRL | Qt::Key_3))));
    m_autoSend->setToolTip(
        QStringLiteral("Auto-send on highlight (%1)").arg(shortcutText(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A))));
    m_system->setToolTip(QStringLiteral("System menu"));

    // Status bar.
    auto *bar = new QStatusBar(this);
    bar->setObjectName(QStringLiteral("statusBar"));
    bar->setSizeGripEnabled(false);
    bar->setFixedHeight(kStatusHeight);
    // The strip is not a layout item: QStatusBar insets its items, and DESIGN 8.1 puts
    // them at exact x and y. It covers the bar below the 1 px top edge (see eventFilter).
    m_statusStrip = new StatusStrip(bar);
    bar->installEventFilter(this);
    setStatusBar(bar);

    // Tab order (DESIGN 9).
    QWidget *order[] = {m_search, m_list, m_viewAll, m_viewFav, m_viewRecent, m_autoSend,
                        m_system, m_groupBar, m_engine, m_grid, m_prev, m_next};
    for (size_t i = 0; i + 1 < sizeof(order) / sizeof(order[0]); ++i)
        setTabOrder(order[i], order[i + 1]);

    // Wiring.
    connect(m_search, &QLineEdit::textChanged, this, &MainWindow::onQueryChanged);
    connect(m_search, &QLineEdit::returnPressed, this, &MainWindow::sendHighlighted);
    connect(m_list, &PatchListView::sendRequested, this, [this](const QModelIndex &) { sendHighlighted(); });
    connect(m_list, &PatchListView::starClicked, this,
            [this](const QModelIndex &index) { toggleFavorite(m_model->addressAt(index)); });
    connect(m_list, &PatchListView::favoriteToggleRequested, this,
            [this](const QModelIndex &index) {
                if (index.isValid())
                    toggleFavorite(m_model->addressAt(index));
                else
                    toggleHighlightedFavorite();
            });
    connect(m_delegate, &PatchDelegate::nameCommitted, this, &MainWindow::onNameCommitted);
    connect(&m_library, &Library::userNamesChanged, this, [this](Model) { refreshDisplayPatch(); });
    connect(m_list, &PatchListView::rowClicked, this, [this](const QModelIndex &index) {
        sendAddress(m_model->addressAt(index));
    });
    connect(m_model, &QAbstractItemModel::modelAboutToBeReset, this, &MainWindow::onModelAboutToReset);
    connect(m_model, &QAbstractItemModel::modelReset, this, &MainWindow::onModelReset);
    // A first send in the Recents view inserts a row (possibly into an empty list).
    connect(m_model, &QAbstractItemModel::rowsInserted, this, [this] {
        if (m_shown == Model::D05)
            m_display->setSearch(m_model->searchActive(), m_model->rowCount());
        m_list->setEmptyText(m_model->emptyTitle(), m_model->emptyNote());
        updateLedDisplay();
    });

    connect(m_prev, &QAbstractButton::clicked, this, [this] { step(-1); });
    connect(m_next, &QAbstractButton::clicked, this, [this] { step(1); });
    connect(m_viewGroup, &QButtonGroup::buttonToggled, this, [this](QAbstractButton *, bool on) {
        if (on)
            onViewToggled();
    });
    connect(m_autoSend, &QAbstractButton::toggled, this, [this](bool on) { setAutoSend(on); });
    connect(m_list, &PatchListView::highlightMovedByKey, this, &MainWindow::onHighlightMovedByKey);
    connect(m_system, &QAbstractButton::clicked, this, &MainWindow::popupSystemMenu);

    connect(m_groupBar, &GroupBar::groupPressed, this, [this](int group) {
        m_grid->setFocus(Qt::OtherFocusReason);
        m_grid->beginEntry(group);
    });
    connect(m_grid, &BankGrid::patchChosen, this, [this](const PatchAddress &a) { sendAddress(a); });
    connect(m_grid, &BankGrid::entryChanged, this, &MainWindow::onEntryChanged);
    connect(m_engine, &EngineControl::engineRequested, this, &MainWindow::onEngineRequested);

    // Every link: what it sent (a selection, a flip confirmation, a resend or TEST), and, for the two
    // devices that follow with a full address, a panel change. The JU-06A's follow is the tracker's.
    for (Model m : allModels())
        connect(m_router->link(m), &MidiLink::sent, this, &MainWindow::onSent);
    connect(&m_link, &MidiLink::currentChanged, this, &MainWindow::onFollowed);
    connect(m_router->link(Model::JP08), &MidiLink::currentChanged, this, &MainWindow::onFollowed);
    // (The router's own slot ran first: with FR-20 on the JU-06A is already the current device here.)
    connect(m_router->link(Model::JU06A), &MidiLink::programReceived, this, [this](int) {
        if (currentDevice() != Model::JU06A)
            return;  // another device is shown: nothing of the JU-06A's event touches its UI
        m_sendFlash = false;
        m_display->flashActivity();
        updateLedDisplay();
    });
    // Every device's link: connection changes and errors (an error is told in the status bar only
    // for the device the window shows).
    for (Model m : allModels()) {
        MidiLink *l = m_router->link(m);
        connect(l, &MidiLink::connectedChanged, this, [this, m](bool connected) { onConnectedChanged(m, connected); });
        connect(l, &MidiLink::inputStatusChanged, this, [this, m](const QString &status) {
            if (m == currentDevice() && status.startsWith(QStringLiteral("Error")))
                showMessage(QStringLiteral("Input: ") + status);
            refreshConnection();
        });
        connect(l, &MidiLink::statusChanged, this, [this, m](const QString &status) {
            if (m == currentDevice() && status.startsWith(QStringLiteral("Error")))
                showMessage(status);
            refreshConnection();
        });
    }
    connect(m_router, &DeviceRouter::currentDeviceChanged, this, &MainWindow::onDeviceChanged);
    connect(m_switcher, &DeviceSwitcher::switchRequested, this, &MainWindow::switchDevice);
    connect(m_display, &DisplayWidget::sizeChanged, this, [this] {
        if (!m_inLayout)
            layoutAll();  // a DPR or screen change moved the band without a window resize
    });
    connect(m_led, &LedDisplayWidget::sizeChanged, this, [this] {
        if (!m_inLayout)
            layoutAll();
    });
    connect(Theme::notifier(), &Theme::Notifier::skinChanged, this, [this] { applySkin(); });
    // The JU-06A skin follows the engine the app believes (FR-17), whichever device is current; the
    // engine control, the list markers, the display and the grid follow its arm, hint and patches.
    {
        const EngineTracker *t = m_router->ju06aTracker();
        connect(t, &EngineTracker::engineChanged, this, &MainWindow::onEngineChanged);
        connect(t, &EngineTracker::armedChanged, this, [this] { syncEngineState(); });
        connect(t, &EngineTracker::hintChanged, this, [this](int) { syncEngineState(); });
        connect(t, &EngineTracker::rememberedChanged, this, [this] { syncEngineState(); });
        connect(t, &EngineTracker::currentChanged, this, [this] {
            if (currentDevice() == Model::JU06A)
                showCurrentPatch(true);
        });
    }
    connect(&m_library, &Library::saveFailed, this,
            [this](const QString &text) { showMessage(QStringLiteral("Library not saved: ") + text); });
    connect(m_display, &DisplayWidget::sentChanged, this, [this](bool on) {
        static_cast<StatusStrip *>(m_statusStrip)->led->setLit(on);
        if (!on)
            m_sendFlash = false;
        updateLedDisplay();  // SENT is on the LED display's context line for as long as the flash
    });
}

void MainWindow::buildActions()
{
    // Keys come from the one table the shortcut sheet is generated from (ui/Shortcuts).
    const auto make = [this](const QString &name, const QString &text, auto slot) {
        auto *a = new QAction(text, this);
        a->setObjectName(name);
        const QKeySequence key = Shortcuts::key(name);
        if (!key.isEmpty())
            a->setShortcut(key);
        a->setShortcutContext(Qt::WindowShortcut);
        connect(a, &QAction::triggered, this, slot);
        addAction(a);
        return a;
    };

    make(QStringLiteral("actionFocusSearch"), QStringLiteral("Find"), [this] { focusSearch(); });
    make(QStringLiteral("actionFocusSearchCtrlF"), QStringLiteral("Find"), [this] { focusSearch(); });
    make(QStringLiteral("actionEscape"), QStringLiteral("Clear search"), [this] { onEscape(); });
    // Every trigger sends MIDI: holding the key must not stream patches at the synth.
    make(QStringLiteral("actionPrev"), QStringLiteral("Previous patch"), [this] { step(-1); })
        ->setAutoRepeat(false);
    make(QStringLiteral("actionNext"), QStringLiteral("Next patch"), [this] { step(1); })
        ->setAutoRepeat(false);
    make(QStringLiteral("actionGridEntry"), QStringLiteral("Enter bank and number"), [this] { beginGridEntry(); });
    make(QStringLiteral("actionViewAll"), QStringLiteral("All"), [this] { m_viewAll->setChecked(true); });
    make(QStringLiteral("actionViewFavourites"), QStringLiteral("Favourites"), [this] { m_viewFav->setChecked(true); });
    make(QStringLiteral("actionViewRecents"), QStringLiteral("Recents"), [this] { m_viewRecent->setChecked(true); });
    make(QStringLiteral("actionToggleFavourite"), QStringLiteral("Toggle favourite"), [this] { toggleHighlightedFavorite(); })
        ->setAutoRepeat(false);
    make(QStringLiteral("actionRename"), QStringLiteral("Rename"), [this] { renameHighlighted(); })
        ->setAutoRepeat(false);
    make(QStringLiteral("actionAutoSend"), QStringLiteral("Auto-send"), [this] { setAutoSend(!m_autoSendOn); })
        ->setAutoRepeat(false);
    // Device switching and the JU-06A engine toggle (FR-13, DESIGN-2.md 4.4, 13.2.1) are table actions
    // like the rest, so the sheet's DEVICE section cannot list a key the window lacks. Command+Tab
    // belongs to the system on macOS, so there the device keys are Control+Tab (Qt's Meta). Qt delivers
    // Shift+Tab as Backtab, so the previous-device action answers to both.
    make(QStringLiteral("actionNextDevice"), QStringLiteral("Next device"), [this] { cycleDevice(1); })
        ->setAutoRepeat(false);
    {
        QAction *prevDevice =
            make(QStringLiteral("actionPreviousDevice"), QStringLiteral("Previous device"), [this] { cycleDevice(-1); });
        prevDevice->setAutoRepeat(false);
#ifdef Q_OS_MACOS
        constexpr int mod = Qt::META;
#else
        constexpr int mod = Qt::CTRL;
#endif
        prevDevice->setShortcuts({Shortcuts::key(QStringLiteral("actionPreviousDevice")),
                                  QKeySequence(mod | Qt::SHIFT | Qt::Key_Backtab)});
    }
    make(QStringLiteral("actionToggleEngine"), QStringLiteral("Toggle JU-06A engine"), [this] { toggleEngine(); })
        ->setAutoRepeat(false);

    // The system menu (DESIGN 8.2). The actions carry their shortcuts, so they are the
    // window's shortcuts for Settings, the sheet and Quit.
    m_systemMenu = new QMenu(this);
    m_systemMenu->setObjectName(QStringLiteral("systemMenu"));
    auto *settings = make(QStringLiteral("actionSettings"), QStringLiteral("Settings..."), [this] { openSettings(); });
    auto *sheet = make(QStringLiteral("actionShortcuts"), QStringLiteral("Keyboard Shortcuts"), [this] { toggleShortcuts(); });
    auto *clear = make(QStringLiteral("actionClearRecents"), QStringLiteral("Clear Recents..."), [this] { confirmClearRecents(); });
    auto *about = make(QStringLiteral("actionAbout"), QStringLiteral("About D-05 Patch Switcher"), [this] { openAbout(); });
    auto *quit = make(QStringLiteral("actionQuit"), QStringLiteral("Quit"), [this] { saveAndQuit(); });
    m_systemMenu->addAction(settings);
    m_systemMenu->addAction(sheet);
    m_systemMenu->addAction(clear);
    m_systemMenu->addSeparator();
    m_systemMenu->addAction(about);
    m_systemMenu->addSeparator();
    m_systemMenu->addAction(quit);

#ifdef Q_OS_MACOS
    // DESIGN 8.2: on macOS the same actions also live in the native menu bar (a parentless
    // QMenuBar); there is no in-window menu bar on any platform.
    settings->setMenuRole(QAction::PreferencesRole);
    about->setMenuRole(QAction::AboutRole);
    quit->setMenuRole(QAction::QuitRole);
    auto *nativeBar = new QMenuBar(nullptr);
    QMenu *appMenu = nativeBar->addMenu(QStringLiteral("D-05 Patch Switcher"));
    appMenu->addActions({settings, sheet, clear, about, quit});
    m_nativeMenuBar = nativeBar;
#endif
}

QAction *MainWindow::action(const QString &name) const
{
    return findChild<QAction *>(name);
}

PanelButton *MainWindow::viewButton(PatchListModel::View view) const
{
    switch (view) {
    case PatchListModel::View::All: return m_viewAll;
    case PatchListModel::View::Favourites: return m_viewFav;
    case PatchListModel::View::Recents: return m_viewRecent;
    }
    return m_viewAll;
}

// ---------------------------------------------------------------------------- layout

// The parts of the window that depend on the skin (DESIGN-2.md 2.6): which display fills the
// slot, whether the group slot holds the group bar, the engine control or nothing, the search
// placeholder. Called once at construction and from Theme::skinChanged(), which Theme::setSkin() emits;
// the style sheet change and this method run in one event-loop turn, so the window paints once. The style
// sheet and the painted widgets follow the palette on their own.
void MainWindow::applySkin()
{
    const Theme::Skin &skin = Theme::skin();
    const bool led = skin.display == Theme::DisplayKind::Led;
    // The geometry first (it sizes and builds the LED display's atlas once, while it is still
    // hidden), then the visibility and the content.
    m_groupBar->setVisible(skin.groupSlot == Theme::GroupSlot::GroupBar && d05Current());
    m_engine->setVisible(skin.groupSlot == Theme::GroupSlot::Engine && currentDevice() == Model::JU06A);
    m_search->setPlaceholderText(skin.searchPlaceholder);
    layoutAll();
    m_display->setVisible(!led);
    m_led->setVisible(led);
    if (led)
        updateLedDisplay();
    update();
}

// The skin of the current device (DESIGN-2.md 2.1): the model's, and for the JU-06A the engine the
// app believes the unit is on.
QString MainWindow::skinIdForCurrent() const
{
    return Theme::skinIdFor(modelInfo(currentDevice()).id, m_router->ju06aTracker()->engine());
}

// A LED skin that is not the current device's: set on its own (tests, tools) while the D-05 is
// current, or the device just changed and its skin is not on yet.
bool MainWindow::ledIsPlaceholder() const
{
    return Theme::skin().model != modelInfo(currentDevice()).id;
}

// The context line of the LED display (DESIGN-2.md 4.2, FR-12), the first that applies.
QString MainWindow::contextLine(LedDisplayWidget::Tone *tone) const
{
#ifdef Q_OS_MACOS
    const QString ctrl = QStringLiteral("CMD");
#else
    const QString ctrl = QStringLiteral("CTRL");
#endif
    const Model m = currentDevice();
    const MidiLink *l = m_router->link(m);
    const EngineTracker *t = m_router->ju06aTracker();
    *tone = LedDisplayWidget::Tone::Secondary;
    if (l->outputPortName().isEmpty()) {  // 1a: enabled, no output port saved
        *tone = LedDisplayWidget::Tone::Primary;
        return QStringLiteral("OPEN SETTINGS: ") + Shortcuts::settingsHintKey();
    }
    if (!l->isConnected()) {  // 1b
        *tone = LedDisplayWidget::Tone::Red;
        return QStringLiteral("NO MIDI OUTPUT");
    }
    if (m == Model::JU06A && t->armed()) {  // 2: the prompt belongs to the unit, so it is in the readout's colour
        *tone = LedDisplayWidget::Tone::Lit;
        const int x = t->armed()->engine();
        return t->prompt() == EngineTracker::Prompt::FlipSwitch
                   ? QStringLiteral("FLIP SWITCH TO %1").arg(x)
                   : QStringLiteral("FLIP TO %1, THEN %2+E").arg(x).arg(ctrl);
    }
    if (m_sendFlash && m_sentOn == m) {  // 3
        *tone = LedDisplayWidget::Tone::Primary;
        return QStringLiteral("SENT");
    }
    if (m == Model::JU06A && t->hint() != 0) {  // 4
        *tone = LedDisplayWidget::Tone::Primary;
        return QStringLiteral("FLIPPED TO %1? %2+E").arg(t->hint()).arg(ctrl);
    }
    if (m_model->searchActive()) {  // 5
        const int n = m_model->rowCount();
        return n == 0 ? QStringLiteral("NO MATCHES")
                      : (n == 1 ? QStringLiteral("1 MATCH") : QStringLiteral("%1 MATCHES").arg(n));
    }
    return Theme::skin().idleContext;  // 6
}

// What the LED display shows: the readout, the name and the context line of the current device.
// A skin that is not the device's keeps the idle line it always showed (dashes, no name).
void MainWindow::updateLedDisplay()
{
    if (Theme::skin().display != Theme::DisplayKind::Led)
        return;
    const Theme::Skin &skin = Theme::skin();
    if (ledIsPlaceholder()) {
        m_led->setContent(QString(skin.ledCells, QLatin1Char('-')), QString(), skin.idleContext,
                          LedDisplayWidget::Tone::Secondary);
        return;
    }
    const Model m = currentDevice();
    QString readout, name;
    const bool ju = m == Model::JU06A;
    if (m_grid->stage() != BankGrid::Stage::Idle && skin.displayEntryEcho) {
        readout = m_grid->entryText();  // "__" / "4_" (JP-08), "_-_" / "4-_" (JU-06A); no name while entering
    } else if (const std::optional<PatchAddress> cur = currentOf(m)) {
        readout = ju ? QStringLiteral("%1-%2").arg(cur->bank()).arg(cur->number())
                     : QStringLiteral("%1%2").arg(cur->bank()).arg(cur->number());
        name = m_catalogs[int(m)]->name(*cur);
    } else {
        readout = QString(skin.ledCells, QLatin1Char('-'));  // no patch on that engine
    }
    LedDisplayWidget::Tone tone;
    const QString context = contextLine(&tone);
    m_led->setContent(readout, name, context, tone);
}

void MainWindow::layoutAll()
{
    const QScopedValueRollback<bool> guard(m_inLayout, true);
    const int W = width();
    const int H = height();

    // The slot holds the LCD or the LED display, whichever the skin names; both report the same
    // size, so nothing below moves when the skin changes (DESIGN-2.md 4.1).
    const bool led = Theme::skin().display == Theme::DisplayKind::Led;
    const int step = DisplayWidget::pitchForWidth(W);
    QWidget *active = led ? static_cast<QWidget *>(m_led) : static_cast<QWidget *>(m_display);
    if (led)
        m_led->setStep(step, W);
    else
        m_display->setStep(step, W);
    const QSize ds = active->sizeHint();
    active->setGeometry(kMargin, 46, ds.width(), ds.height());
    // The lone nameplate keeps its 360 px slot; the tabs of two or three devices need the row.
    m_switcher->setGeometry(kMargin, 10, m_switcher->hasTabs() ? W - 2 * kMargin : qMin(Nameplate::SlotMaxWidth, W - 2 * kMargin),
                            Nameplate::SlotHeight);

    const int B = 46 + ds.height() + 14;
    m_bandTop = B;
    m_band->setGeometry(0, B, W, H - kStatusHeight - B);
    m_band->lower();

    // Left column (DESIGN 4.3): faces at x = 16 + 50 col.
    m_groupBar->move(kMargin - 4, B + 12);
    m_engine->move(kMargin - 4, B + 12);
    m_grid->move(kMargin - 4, B + 92);
    m_prev->setGeometry(PanelButton::widgetRectForFace(QPoint(kMargin, B + 231)));
    m_next->setGeometry(PanelButton::widgetRectForFace(QPoint(kMargin + 50, B + 231)));

    // Right column.
    const int X = kColumnX;
    const int R = W - 450;
    const int facesX[] = {X, X + 72, X + 150, X + R - 124, X + R - 44};
    PanelButton *controls[] = {m_viewAll, m_viewFav, m_viewRecent, m_autoSend, m_system};
    for (int i = 0; i < 5; ++i)
        controls[i]->setGeometry(PanelButton::widgetRectForFace(QPoint(facesX[i], B + 28)));
    m_search->setGeometry(X, B + 64, R, 32);
    m_list->setGeometry(X, B + 104, R, H - kStatusHeight - B - 116);

    auto *band = static_cast<Band *>(m_band);
    const QColor &primary = Theme::colors().labelPrimary;
    const QColor &commit = Theme::colors().labelCommit;
    band->setLabels({
        {QStringLiteral("ALL"), facesX[0] + 22, 15 + 9, primary},
        {QStringLiteral("FAVOURITES"), facesX[1] + 22, 15 + 9, primary},
        {QStringLiteral("RECENTS"), facesX[2] + 22, 15 + 9, primary},
        {QStringLiteral("AUTO SEND"), facesX[3] + 22, 15 + 9, commit},
        {QStringLiteral("SYSTEM"), facesX[4] + 22, 15 + 9, primary},
        {QStringLiteral("PREV"), kMargin + 22, 218 + 9, primary},
        {QStringLiteral("NEXT"), kMargin + 50 + 22, 218 + 9, primary},
    });
    relayoutStatus();
}

void MainWindow::relayoutStatus()
{
    m_statusStrip->setGeometry(0, 1, width(), kStatusHeight - 1);
    static_cast<StatusStrip *>(m_statusStrip)->relayout();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    layoutAll();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    layoutAll();
    if (m_firstShow) {
        m_firstShow = false;
        highlightCurrent(true);
    }
}

// ---------------------------------------------------------------------------- events

bool MainWindow::event(QEvent *e)
{
    if (e->type() == QEvent::WindowActivate) {
        m_router->checkConnections();  // FR-10: the only check while connected (no timer)
        if (!anyDeviceEnabled()) {
            refreshDetection();  // the port list is read again: a unit plugged in since is found
            updateDisplayConnection();
        }
    }
    return QMainWindow::event(e);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *e)
{
    if (e->type() == QEvent::Resize && watched == statusBar()) {
        m_statusStrip->setGeometry(0, 1, statusBar()->width(), statusBar()->height() - 1);
    } else if (e->type() == QEvent::MouseButtonPress && m_grid->stage() != BankGrid::Stage::Idle) {
        // Application-wide filter, installed only while an entry is pending (DESIGN 6.4):
        // a press on anything in this window but the grid cancels the entry.
        const auto *w = qobject_cast<QWidget *>(watched);
        if (w && w->window() == this && w != m_grid && !m_grid->isAncestorOf(w))
            m_grid->cancelEntry();
    } else if (watched == m_search && e->type() == QEvent::KeyPress) {
        const auto *ke = static_cast<QKeyEvent *>(e);
        if (!ke->isAutoRepeat())
            m_searchRepeatKey = 0;  // a fresh press (this key or any other) ends a stale hold
        if (ke->isAutoRepeat() && (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter))
            return true;  // a held Return in the search field sends once, not per repeat
        if (ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down) {
            const std::optional<PatchAddress> before = highlighted();
            moveHighlight(ke->key() == Qt::Key_Up ? -1 : 1);
            if (m_autoSendOn && highlighted() != before) {
                // As in the list: a held key reports only its first step and, on its release, its last.
                if (ke->isAutoRepeat())
                    m_searchRepeatKey = ke->key();
                else
                    sendHighlighted();
            }
            return true;
        }
    } else if (watched == m_search && e->type() == QEvent::KeyRelease) {
        const auto *ke = static_cast<QKeyEvent *>(e);
        if (!ke->isAutoRepeat() && m_searchRepeatKey != 0 && ke->key() == m_searchRepeatKey) {
            m_searchRepeatKey = 0;
            if (m_autoSendOn)
                sendHighlighted();
        }
    } else if (watched == m_search && e->type() == QEvent::FocusOut) {
        m_searchRepeatKey = 0;  // the release will go elsewhere
    }
    return QMainWindow::eventFilter(watched, e);
}

// ---------------------------------------------------------------------------- sending

bool MainWindow::sendAddress(const PatchAddress &address)
{
    const Model m = address.model();
    // The D-05 sends through its link as in version 1. The others go through the router: the JP-08
    // sends, the JU-06A's tracker sends or, for the other engine, arms (nothing is sent then).
    const bool ok = m == Model::D05 ? m_link.sendPatch(address) : m_router->select(address);
    if (!ok) {
        showMessage(QStringLiteral("Not sent: ") + m_router->link(m)->status());
        showCurrentPatch(false);  // end any entry, back to the current patch
        return false;
    }
    if (m == currentDevice() && m_grid->stage() != BankGrid::Stage::Idle)
        showCurrentPatch(false);  // an armed selection sends nothing: the entry ends here
    return true;
}

void MainWindow::sendHighlighted()
{
    const auto a = highlighted();
    if (a)
        sendAddress(*a);
}

// PREV / NEXT (FR-5): one step in catalogue order of the device (the JU-06A within its believed
// engine, clamped), then a selection like any other. With no patch (the JU-06A after a flip into
// an engine that shows ---) either key selects 1-1 of the believed engine.
void MainWindow::step(int direction)
{
    const Model m = currentDevice();
    const std::optional<PatchAddress> cur = currentOf(m);
    if (cur) {
        sendAddress(direction < 0 ? PatchCatalog::prev(*cur) : PatchCatalog::next(*cur));
    } else if (m == Model::JU06A) {
        sendAddress(*PatchAddress::fromBankNumber(Model::JU06A, 1, 1, m_router->ju06aTracker()->engine()));
    }
}

void MainWindow::beginGridEntry()
{
    m_grid->setFocus(Qt::ShortcutFocusReason);
    m_grid->beginEntry(d05Current() ? m_current.group() : 0);
}

void MainWindow::onSent(const PatchAddress &address)
{
    // A resend or a TEST is not a selection: no Recents entry, and it leaves an open rename
    // editor and the highlight alone.
    if (!m_quietSend) {
        finishEditing();
        m_library.recordSend(address);
    }
    m_rt[int(address.model())].resendBlocked = false;  // a send worked: resend-on-connect may try again
    m_sentOn = address.model();
    // The JU-06A's current patch is committed by its tracker once the send is on the wire; the
    // others' is the address that went out.
    if (address.model() != Model::JU06A)
        applyCurrent(address, !m_quietSend);
    if (address.model() == Model::D05) {
        m_display->flashSent();
    } else {
        // The LED skins show SENT on their own context line; the one 400 ms timer and the status LED are
        // the D-05 display's (a flash of activity, which does not depend on the D-05's connection).
        m_sendFlash = true;
        m_display->flashActivity();
        updateLedDisplay();
    }
}

// An incoming Bank Select / Program Change moved a device's current patch (follow, FR-11): every
// view of that device follows, the activity LED blinks, and nothing is sent, so auto-send cannot fire.
void MainWindow::onFollowed(const PatchAddress &address)
{
    // The link announces the patch before the router decides on a FR-20 switch, so a device that
    // is not shown yet only has its state applied here; if it becomes current, onDeviceChanged()
    // finishes the editing and does the SENT and activity part.
    if (address.model() != currentDevice()) {
        applyCurrent(address);
        return;
    }
    finishEditing();
    m_sendFlash = false;  // a followed patch is not a send: it ends a SENT flash
    applyCurrent(address);
    m_display->flashActivity();
}

// A device's current patch changed (sent, followed or restored). The D-05's and the JP-08's are kept
// here; when the device is the one shown, every view of it follows.
void MainWindow::applyCurrent(const PatchAddress &address, bool moveHighlight)
{
    switch (address.model()) {
    case Model::D05:
        m_current = address;
        m_display->setPatch(address, m_catalog.name(address), m_catalog.mode(address));
        m_groupBar->setLitGroup(address.group());
        break;
    case Model::JP08:
        m_jpCurrent = address;
        break;
    case Model::JU06A:
        return;  // the tracker's
    }
    if (address.model() == currentDevice())
        showCurrentPatch(moveHighlight);
}

// The shown device's current patch into the list, the grid and the displays.
void MainWindow::showCurrentPatch(bool moveHighlight)
{
    const Model m = currentDevice();
    if (m != m_shown)
        return;  // the list still holds the previous device: enterDevice() fills it
    if (const std::optional<PatchAddress> cur = currentOf(m)) {
        m_model->setCurrent(*cur);
        m_grid->setCurrent(*cur);
        if (moveHighlight)
            highlightCurrent(false);
    } else {
        m_model->clearCurrent();
        m_grid->clearCurrent();
    }
    updateLedDisplay();
}

void MainWindow::setCurrent(const PatchAddress &address)
{
    m_router->link(address.model())->setCurrent(address);
    applyCurrent(address);
}

std::optional<PatchAddress> MainWindow::highlighted() const
{
    const QModelIndex i = m_list->currentIndex();
    if (!i.isValid())
        return std::nullopt;
    return m_model->addressAt(i);
}

// ---------------------------------------------------------------------------- search, views, list

void MainWindow::onQueryChanged(const QString &text)
{
    const bool wasSearching = m_model->searchActive();
    if (!SearchQuery(text, m_shown).isEmpty()) {
        if (!wasSearching)
            m_preSearch = highlighted();  // where the user was before searching
        m_restore = Restore::First;
    } else {
        m_restore = Restore::BeforeSearch;
    }
    m_model->setQuery(text);
    m_restore = Restore::Keep;
}

void MainWindow::onViewToggled()
{
    finishEditing();
    PatchListModel::View v = PatchListModel::View::All;
    if (m_viewFav->isChecked())
        v = PatchListModel::View::Favourites;
    else if (m_viewRecent->isChecked())
        v = PatchListModel::View::Recents;
    m_restore = Restore::Highlight;
    m_model->setView(v);
    m_restore = Restore::Keep;
}

void MainWindow::onModelAboutToReset()
{
    if (m_restore == Restore::Device)
        return;  // enterDevice() has set the highlight to come back to
    m_savedHighlight = highlighted();
    if (m_savedHighlight)
        m_lastHighlight = m_savedHighlight;
}

void MainWindow::onModelReset()
{
    if (m_shown == Model::D05)
        m_display->setSearch(m_model->searchActive(), m_model->rowCount());
    m_list->setEmptyText(m_model->emptyTitle(), m_model->emptyNote());
    updateLedDisplay();  // the match count is on its context line
    if (m_model->rowCount() == 0)
        return;
    const std::optional<PatchAddress> cur = currentOf(m_shown);
    switch (m_restore) {
    case Restore::First:
        highlightRow(0, false);
        m_list->verticalScrollBar()->setValue(0);
        break;
    case Restore::Highlight:
        // Switching views keeps the highlighted patch if the new view has it.
        if (m_savedHighlight && m_model->rowOf(*m_savedHighlight) >= 0)
            highlightRow(m_model->rowOf(*m_savedHighlight), true);
        else if (cur && m_model->rowOf(*cur) >= 0)
            highlightCurrent(true);
        else
            highlightRow(0, false);
        break;
    case Restore::BeforeSearch:
        // The search was cleared: back to the row the user had, else the current patch.
        if (m_preSearch && m_model->rowOf(*m_preSearch) >= 0)
            highlightRow(m_model->rowOf(*m_preSearch), true);
        else if (cur && m_model->rowOf(*cur) >= 0)
            highlightCurrent(true);
        else
            highlightRow(0, false);
        break;
    case Restore::Device:
        // Back to the row this device's list had when the window left it, else the current patch.
        if (m_savedHighlight && m_model->rowOf(*m_savedHighlight) >= 0)
            highlightRow(m_model->rowOf(*m_savedHighlight), true);
        else if (cur && m_model->rowOf(*cur) >= 0)
            highlightCurrent(true);
        else
            highlightRow(0, false);
        break;
    case Restore::Keep:
        if (m_savedHighlight && m_model->rowOf(*m_savedHighlight) >= 0)
            highlightRow(m_model->rowOf(*m_savedHighlight), false);
        else
            highlightRow(0, false);
        break;
    }
}

void MainWindow::highlightRow(int row, bool scrollNearTop)
{
    if (row < 0 || row >= m_model->rowCount())
        return;
    const QModelIndex i = m_model->index(row);
    m_list->setCurrentIndex(i);
    if (scrollNearTop) {
        // The current patch sits three rows below the top, as in the mockup (DESIGN 14.2).
        m_list->scrollTo(i, QAbstractItemView::PositionAtTop);
        QScrollBar *sb = m_list->verticalScrollBar();
        sb->setValue(qMax(sb->minimum(), sb->value() - 3));
    } else {
        m_list->scrollTo(i, QAbstractItemView::EnsureVisible);
    }
}

void MainWindow::highlightCurrent(bool scrollNearTop)
{
    const std::optional<PatchAddress> cur = currentOf(m_shown);
    if (!cur)
        return;
    const int row = m_model->rowOf(*cur);
    if (row >= 0)
        highlightRow(row, scrollNearTop);
}

void MainWindow::moveHighlight(int delta)
{
    const int rows = m_model->rowCount();
    if (rows == 0)
        return;
    const int cur = m_list->currentIndex().isValid() ? m_list->currentIndex().row() : -1;
    highlightRow(qBound(0, cur + delta, rows - 1), false);
}

// ---------------------------------------------------------------------------- library actions

void MainWindow::toggleFavorite(const PatchAddress &address)
{
    finishEditing();
    const std::optional<PatchAddress> before = highlighted();
    const int oldRow = m_list->currentIndex().isValid() ? m_list->currentIndex().row() : 0;
    m_library.toggleFavorite(address);
    // In the Favourites view an un-starred row leaves the list; keep the highlight at the
    // same place rather than jumping to the top.
    if (before && m_model->rowOf(*before) < 0 && m_model->rowCount() > 0)
        highlightRow(qMin(oldRow, m_model->rowCount() - 1), false);
}

void MainWindow::toggleHighlightedFavorite()
{
    // An empty list (no favourites yet) toggles the last highlighted patch, else the current one.
    std::optional<PatchAddress> a = highlighted();
    if (!a && m_lastHighlight && m_lastHighlight->model() == m_shown)
        a = m_lastHighlight;
    if (!a)
        a = currentOf(m_shown);
    if (a)
        toggleFavorite(*a);
}

void MainWindow::renameHighlighted()
{
    const QModelIndex i = m_list->currentIndex();
    if (!i.isValid())
        return;
    if (!PatchListModel::canRename(m_model->addressAt(i))) {
        showMessage(QStringLiteral("Only user slots can be renamed"));
        return;
    }
    m_list->scrollTo(i);
    m_list->edit(i);
}

void MainWindow::onNameCommitted(const PatchAddress &address, const QString &text)
{
    // Deferred one turn: with a search active the rename rebuilds the list, and that must
    // not happen under the view while it is still closing the editor.
    flushPendingRename();  // an earlier one that is still queued goes first
    m_pendingRename = qMakePair(address, text);
    QMetaObject::invokeMethod(
        this,
        [this] {
            flushPendingRename();
            // Enter and Esc leave the focus with the list; a commit by clicking elsewhere
            // must leave it where the user put it.
            if (!QApplication::focusWidget())
                m_list->setFocus(Qt::OtherFocusReason);
        },
        Qt::QueuedConnection);
}

void MainWindow::flushPendingRename()
{
    if (!m_pendingRename)
        return;
    const auto held = *m_pendingRename;
    m_pendingRename.reset();
    m_library.setUserName(held.first, held.second);
}

// Closing or quitting with a rename in progress keeps the name: the open editor is committed
// and a commit still waiting in the queue is applied now, not dropped.
void MainWindow::saveAndQuit()
{
    finishEditing();
    flushPendingRename();
    QApplication::quit();
}

void MainWindow::finishEditing()
{
    if (const auto held = m_list->takeEditor())
        m_library.setUserName(held->first, held->second);
}

void MainWindow::refreshDisplayPatch()
{
    m_display->setPatch(m_current, m_catalog.name(m_current), m_catalog.mode(m_current));
    updateLedDisplay();  // the name of the JP-08's or JU-06A's current patch may have changed
}

void MainWindow::confirmClearRecents()
{
    if (m_clearBox)
        return;  // already asking (or the last box is still being deleted)
    // A plain QDialog like the others (DESIGN 10.5): QMessageBox's fixed text indent and
    // margins put the text and the buttons off the 20 px grid.
    auto *box = new QDialog(this);
    box->setObjectName(QStringLiteral("clearRecentsBox"));
    box->setWindowTitle(QStringLiteral("Clear Recents"));
    box->setAttribute(Qt::WA_DeleteOnClose);
    auto *column = new QVBoxLayout(box);
    column->setContentsMargins(20, 20, 20, 20);
    column->setSpacing(20);
    auto *text = new QLabel(QStringLiteral("Clear the list of recently sent patches?"), box);
    text->setProperty("role", QStringLiteral("body"));
    text->setWordWrap(true);
    text->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    column->addWidget(text);
    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);
    row->addStretch(1);
    // The QSS reaches CLEAR through its object name (DESIGN 12).
    auto *clear = new QPushButton(QStringLiteral("CLEAR"), box);
    clear->setObjectName(QStringLiteral("clearButton"));
    clear->setAutoDefault(false);
    auto *cancel = new QPushButton(QStringLiteral("CANCEL"), box);
    cancel->setObjectName(QStringLiteral("cancelButton"));
    cancel->setDefault(true);
    row->addWidget(clear);
    row->addWidget(cancel);
    column->addLayout(row);
    Theme::applyDialogButtonFonts(box);
    box->setFixedWidth(360);
    box->setFixedHeight(column->totalHeightForWidth(360));
    connect(clear, &QAbstractButton::clicked, this, [this, box] {
        finishEditing();
        m_library.clearRecents(m_shown);  // the shown device's list (Recents are per device)
        box->accept();
    });
    connect(cancel, &QAbstractButton::clicked, box, &QDialog::reject);
    cancel->setFocus();  // initial focus is the safe choice
    m_clearBox = box;
    box->open();
}

void MainWindow::toggleShortcuts()
{
    if (m_shortcutsBox) {
        m_shortcutsBox->close();
        return;
    }
    auto *box = new ShortcutsDialog(this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    m_shortcutsBox = box;
    box->open();
}

void MainWindow::openAbout()
{
    if (m_aboutBox)
        return;
    auto *box = new AboutDialog(QCoreApplication::applicationVersion(), this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    m_aboutBox = box;
    box->open();
}

void MainWindow::focusSearch()
{
    m_search->setFocus(Qt::ShortcutFocusReason);
    m_search->selectAll();
}

void MainWindow::onEscape()
{
    // Esc first ends the JU-06A's armed selection and its flip hint (FR-13, FR-19), and is then done;
    // with nothing armed it goes on as before.
    if (currentDevice() == Model::JU06A && m_router->ju06aTracker()->escape())
        return;
    if (m_grid->stage() != BankGrid::Stage::Idle) {
        m_grid->cancelEntry();
        m_list->setFocus(Qt::ShortcutFocusReason);
    } else if (!m_search->text().isEmpty()) {
        m_search->clear();
    } else {
        m_list->setFocus(Qt::ShortcutFocusReason);
    }
}

void MainWindow::onEntryChanged()
{
    const bool idle = m_grid->stage() == BankGrid::Stage::Idle;
    if (idle)
        qApp->removeEventFilter(this);
    else
        qApp->installEventFilter(this);  // Qt ignores a second install of the same filter
    m_display->setEntry(d05Current() ? m_grid->entryText() : QString());
    m_groupBar->setLitGroup(idle ? m_current.group() : m_grid->entryGroup(), idle);
    updateLedDisplay();  // the entry echo
    if (!idle && !m_grid->hasFocus())
        m_grid->setFocus(Qt::OtherFocusReason);
}

// ---------------------------------------------------------------------------- status

void MainWindow::updateDisplayConnection()
{
    m_display->setConnected(m_link.isConnected());
    // First run (DESIGN 5.2): no output port was ever chosen, so line 2 points at Settings
    // instead of saying NO MIDI OUTPUT. A configured port that is missing keeps that text. With no
    // device enabled at all the line also says which units were found on USB (DESIGN-2.md 13.2.3).
    m_display->setFirstRunHint(m_link.outputPortName().isEmpty() ? firstRunHint() : QString());
    updateLedDisplay();
}

bool MainWindow::anyDeviceEnabled() const
{
    for (Model m : allModels())
        if (m_router->isEnabled(m))
            return true;
    return false;
}

// Reads the port lists through the D-05's link and asks Identify (docs/IDENTIFY.md) which models they
// hold. Runs only with no device enabled (the first-run hint is the only use) and only when the port
// list is read anyway: at construction, on window activation, when a device is turned off. Opens and
// sends nothing; there is no timer.
void MainWindow::refreshDetection()
{
    m_detected.clear();
    m_detectionDone = true;
    if (anyDeviceEnabled())
        return;
    QString err;
    QStringList names = m_link.outputPortNames(&err);
    if (!err.isEmpty())
        names.clear();
    names += m_link.inputPortNames(&err);
    const QList<Identify::Result> found = Identify::identifyPorts(names, m_systemLookup);
    for (Model m : allModels()) {
        for (const Identify::Result &r : found) {
            if (r.model && *r.model == m) {
                m_detected.append(m);
                break;
            }
        }
    }
}

void MainWindow::setSystemLookup(bool on)
{
    if (on == m_systemLookup)
        return;
    m_systemLookup = on;
    m_detectionDone = false;
    if (!anyDeviceEnabled()) {
        refreshDetection();
        updateDisplayConnection();
    }
}

QString MainWindow::firstRunHint() const
{
    if (!anyDeviceEnabled()) {
        if (m_detected.size() == 1)
            return QStringLiteral("FOUND %1 ON USB: OPEN SETTINGS").arg(modelInfo(m_detected.first()).name);
        if (m_detected.size() > 1)
            return QStringLiteral("FOUND %1 UNITS ON USB: OPEN SETTINGS").arg(m_detected.size());
    }
    return QStringLiteral("OPEN SETTINGS: ") + Shortcuts::settingsHintKey();
}

void MainWindow::refreshConnection()
{
    auto *s = static_cast<StatusStrip *>(m_statusStrip);
    if (anyDeviceEnabled())
        m_detectionDone = false;  // read again the next time no device is enabled
    else if (!m_detectionDone)
        refreshDetection();
    const Model cur = currentDevice();
    const MidiLink &link = *m_router->link(cur);
    // A device that is not enabled has no ports (DESIGN-2 13.2.3): OUT None, CH 1, IN None, whatever
    // is saved for it.
    const bool enabled = m_router->isEnabled(cur);
    const QString inputLabel = enabled ? m_rt[int(cur)].inputLabel : QString();
    const bool connected = link.isConnected();
    const QString out = !enabled ? QString() : connected ? link.connectedPortKey() : link.outputPortName();
    const QFontMetrics fm(Theme::font(Theme::Role::StatusValue));
    StatusStrip::setValue(s->port, out.isEmpty() ? QStringLiteral("None") : fm.elidedText(out, Qt::ElideMiddle, 280),
                          out.isEmpty());
    s->port->setToolTip(out);
    s->notFound->setVisible(!connected && !out.isEmpty());
    s->chValue->setText(QString::number(enabled ? link.channel() : 1));
    StatusStrip::setValue(s->inValue,
                          inputLabel.isEmpty() ? QStringLiteral("None") : fm.elidedText(inputLabel, Qt::ElideMiddle, 280),
                          inputLabel.isEmpty());
    // FOLLOW only while the input is really open; follow on with an input that is missing or
    // failed shows NOT FOUND after it, as the output does.
    const bool wantsInput = link.followEnabled() && !inputLabel.isEmpty();
    s->follow->setVisible(wantsInput && link.inputConnected());
    s->inNotFound->setVisible(wantsInput && !link.inputConnected());
    updateDisplayConnection();
    updateSwitcher();
    relayoutStatus();
}

// The switcher's tabs: which devices are enabled, the red of an output that is wanted but not open
// (or has no port), and the tooltips (DESIGN-2.md 4.4). Repaints only on a change.
void MainWindow::updateSwitcher()
{
    std::array<DeviceSwitcher::DeviceState, 3> st;
    for (Model m : allModels()) {
        DeviceSwitcher::DeviceState &d = st[int(m)];
        const DeviceRouter::DeviceConfig cfg = m_router->config(m);
        const MidiLink *l = m_router->link(m);
        d.enabled = cfg.enabled;
        d.red = cfg.enabled && (cfg.outputPort.isEmpty() || !l->isConnected());
        const QString name = modelInfo(m).name;
        if (cfg.outputPort.isEmpty())
            d.tooltip = QStringLiteral("%1: no MIDI output (Ctrl+Tab)").arg(name);
        else
            d.tooltip = QStringLiteral("%1: %2, channel %3 (Ctrl+Tab)")
                            .arg(name, l->isConnected() ? l->connectedPortKey() : MidiLink::portKey(cfg.outputPort))
                            .arg(cfg.channel);
    }
    const bool hadTabs = m_switcher->hasTabs();
    m_switcher->setState(st, currentDevice());
    if (m_switcher->hasTabs() != hadTabs)
        layoutAll();  // the row grows with the tabs
}

void MainWindow::setInputPortLabel(const QString &name)
{
    m_rt[int(Model::D05)].inputLabel = name;
    refreshConnection();
}

void MainWindow::showMessage(const QString &text)
{
    auto *s = static_cast<StatusStrip *>(m_statusStrip);
    s->fullMessage = text;
    relayoutStatus();
}

QString MainWindow::message() const
{
    return static_cast<StatusStrip *>(m_statusStrip)->fullMessage;
}

void MainWindow::onHighlightMovedByKey()
{
    if (m_autoSendOn)
        sendHighlighted();
}

// ---------------------------------------------------------------------------- devices

void MainWindow::switchDevice(Model m)
{
    if (!m_router->isEnabled(m) || m == currentDevice())
        return;
    m_router->setCurrentDevice(m);  // a user switch: clears a JU-06A arm; comes back as onDeviceChanged
}

void MainWindow::cycleDevice(int direction)
{
    const Model cur = currentDevice();
    for (int i = 1; i <= 3; ++i) {
        const Model m = Model((int(cur) + direction * i + 3 * 3) % 3);
        if (m_router->isEnabled(m)) {
            switchDevice(m);
            return;
        }
    }
}

// The window follows the router's current device, whoever caused the switch (the user or a
// follow event, FR-20). It all happens in this one turn of the event loop, so the window paints
// once: first the content of the new device goes in (list, grid, markers), then the skin changes,
// then the switcher and the status bar say what is current.
void MainWindow::onDeviceChanged(Model model, bool byFollow)
{
    Theme::markChildFirst();  // before any content changes: the window must not be the first dirty widget
    finishEditing();
    if (byFollow) {
        m_sendFlash = false;  // the event that switched the window is not a send
        m_display->flashActivity();
    }
    if (m_grid->stage() != BankGrid::Stage::Idle)
        m_grid->cancelEntry();
    // Focus in a part that is about to go hidden is dropped (Qt would hand it to AUTO SEND).
    {
        QWidget *f = QApplication::focusWidget();
        if (f && (f == m_groupBar || f == m_engine))
            f->clearFocus();
    }
    enterDevice(model);
    const QString skinId = skinIdForCurrent();
    const bool skinChanges = Theme::skin().id != skinId;
    Theme::setSkin(skinId);  // the notifier runs applySkin(); a no-op on the same skin
    if (!skinChanges)
        applySkin();  // a skin that was already on still has to take this device's group slot
    refreshConnection();  // status bar, display, switcher
    if (model == Model::D05 || !QApplication::focusWidget() || QApplication::focusWidget()->window() != this)
        m_search->setFocus();
    if (m_settings) {
        m_settings->setCurrentDevice(model);
        noteSettingsWrite();
    }
}

// Points the list, grid, search field and view buttons at `model`'s state. What the list held for the
// device being left (search text, view, highlighted row) is kept for when the window comes back
// (FR-4: views are per device). The list resets once.
void MainWindow::enterDevice(Model model)
{
    DeviceUi &old = m_ui[int(m_shown)];
    old.query = m_search->text();
    old.view = m_model->view();
    old.highlight = highlighted();
    m_shown = model;
    const DeviceUi &ui = m_ui[int(model)];
    m_preSearch.reset();
    m_lastHighlight.reset();
    {
        // The widgets say what the model is about to hold; their signals must not rebuild the list again.
        const QSignalBlocker a(m_search), b(m_viewGroup), c(m_viewAll), d(m_viewFav), e(m_viewRecent);
        m_search->setText(ui.query);
        PanelButton *on = ui.view == PatchListModel::View::Favourites ? m_viewFav
                          : ui.view == PatchListModel::View::Recents ? m_viewRecent
                                                                     : m_viewAll;
        on->setChecked(true);
    }
    m_grid->setDevice(model, model == Model::JU06A ? m_router->ju06aTracker()->engine() : 0);
    m_restore = Restore::Device;
    m_savedHighlight = ui.highlight;
    m_model->setDevice(*m_catalogs[int(model)], ui.query, ui.view);
    m_restore = Restore::Keep;
    syncEngineState();
    showCurrentPatch(false);
}

// What the JU-06A's tracker says into the list markers (the believed engine, the armed row, the other
// engine's remembered patch) and the engine control (DESIGN-2.md 4.3, 6.2, 6.3), and the display's
// context line. The list only carries them while the JU-06A's catalogue is shown.
void MainWindow::syncEngineState()
{
    const EngineTracker *t = m_router->ju06aTracker();
    const int armedEngine = t->armed() ? t->armed()->engine() : 0;
    m_engine->setState(t->engine(), armedEngine != 0 ? armedEngine : t->hint());
    if (m_shown == Model::JU06A) {
        m_model->setBelievedEngine(t->engine());
        m_model->setArmed(t->armed());
        const int other = t->remembered(t->otherEngine());
        std::optional<PatchAddress> remembered;
        if (other >= 0)
            remembered = PatchAddress::fromBankNumber(Model::JU06A, other / 8 + 1, other % 8 + 1, t->otherEngine());
        m_model->setOtherRemembered(remembered);
    } else {
        m_model->setArmed(std::nullopt);
        m_model->setOtherRemembered(std::nullopt);
    }
    updateLedDisplay();
}

// The JU-06A's believed engine changed (Ctrl+E, an engine button, a confirmed flip): the skin follows
// it while the JU-06A is the current device (FR-17), and the grid sends on the new engine.
void MainWindow::onEngineChanged(int engine)
{
    if (m_settings)
        m_engineChanged = true;  // after startup: the user's action or a confirmed flip
    if (m_shown == Model::JU06A || currentDevice() == Model::JU06A)
        Theme::markChildFirst();  // before any content changes (see onDeviceChanged)
    if (m_shown == Model::JU06A) {
        // setDevice() would drop a half-typed entry silently; end it properly so that the display,
        // the group bar and the event filter hear about it.
        if (m_grid->stage() != BankGrid::Stage::Idle)
            m_grid->cancelEntry();
        m_grid->setDevice(Model::JU06A, engine);
        m_model->setBelievedEngine(engine);
    }
    syncEngineState();
    if (currentDevice() == Model::JU06A) {
        showCurrentPatch(false);
        Theme::setSkin(skinIdForCurrent());
    }
}

void MainWindow::onEngineRequested(int engine)
{
    if (engine != m_router->ju06aTracker()->engine())
        m_router->ju06aTracker()->toggleEngine();
}

void MainWindow::toggleEngine()
{
    if (currentDevice() == Model::JU06A)
        m_router->ju06aTracker()->toggleEngine();
}

// A current device that is not enabled (the user turned it off in Settings, or none was ever
// turned on) gives way to the first enabled one; with none enabled the D-05 skin shows.
void MainWindow::ensureCurrentDevice()
{
    if (m_router->isEnabled(currentDevice()))
        return;
    for (Model m : allModels()) {
        if (m_router->isEnabled(m)) {
            m_router->setCurrentDevice(m);
            return;
        }
    }
    if (currentDevice() != Model::D05)
        m_router->setCurrentDevice(Model::D05);
}

void MainWindow::onConnectedChanged(Model model, bool connected)
{
    updateDisplayConnection();
    refreshConnection();
    if (connected)
        noteConnected(model);
    else
        ++m_rt[int(model)].connGeneration;
}

void MainWindow::noteConnected(Model model)
{
    ++m_rt[int(model)].connGeneration;
    if (m_settingsOpen)
        m_sess[int(model)].connectPending = true;  // decided when the dialog closes
    else if (m_router->config(model).resendOnConnect)
        resendCurrent(model);
}

// Resend-on-connect: the output (re)connected, so tell the synth what the app shows, once.
// A failed resend is not retried on every poll reconnect: it stays blocked until a send works
// or the settings change.
void MainWindow::resendCurrent(Model model)
{
    DeviceRuntime &rt = m_rt[int(model)];
    if (rt.resendBlocked)
        return;
    if (!sendQuiet(model))
        rt.resendBlocked = true;
}

// A send that is not a selection: no Recents entry. The D-05 resends the patch the window shows;
// the JP-08 resends the patch its link last sent or followed; the JU-06A the tracker's current patch
// (its link keeps only what was sent, the tracker also follows the panel), and only when it is for the
// engine the app believes, so a resend never arms a prompt. Nothing known is not a failure.
bool MainWindow::sendQuiet(Model model)
{
    if (model == Model::D05) {
        m_quietSend = true;
        const bool ok = sendAddress(m_current);
        m_quietSend = false;
        return ok;
    }
    const std::optional<PatchAddress> cur =
        model == Model::JU06A ? m_router->ju06aTracker()->current() : m_router->link(model)->current();
    if (!cur)
        return true;
    if (model == Model::JU06A && cur->engine() != m_router->ju06aTracker()->engine())
        return true;
    const QScopedValueRollback<bool> quiet(m_quietSend, true);
    return m_router->select(*cur);
}

void MainWindow::notAvailable(const QString &what)
{
    showMessage(what + QStringLiteral(" is not available yet"));
}

void MainWindow::popupSystemMenu()
{
    // The menu's right edge lines up with the button's right edge, just below the face.
    const QPoint faceBottomRight =
        m_system->mapToGlobal(QPoint(PanelButton::Margin + PanelButton::FaceW, PanelButton::Margin + PanelButton::FaceH + 2));
    const int w = m_systemMenu->sizeHint().width();
    m_systemMenu->popup(QPoint(faceBottomRight.x() - w, faceBottomRight.y()));
}

// ---------------------------------------------------------------------------- settings

void MainWindow::setServices(AppSettings *settings)
{
    m_settings = settings;
}

DeviceRouter::DeviceConfig MainWindow::configFor(const AppSettings::DeviceValues &v)
{
    DeviceRouter::DeviceConfig c;
    c.enabled = v.enabled;
    c.outputPort = v.outputPort;
    c.inputPort = v.inputPort;
    c.channel = v.channel;
    c.follow = v.follow;
    c.resendOnConnect = v.resendOnConnect;
    return c;
}

AppSettings::Values MainWindow::currentSettings() const
{
    AppSettings::Values v;
    // The key, or the full name when two ports share it (so Cancel restores the right device).
    v.outputPort = m_link.isConnected() ? MidiLink::portSpec(m_link.connectedPortName(), m_link.outputPortNames(nullptr))
                                        : m_link.outputPortName();
    v.inputPort = m_rt[int(Model::D05)].inputLabel;
    v.channel = m_link.channel();
    v.autoSend = m_autoSendOn;
    v.follow = m_link.followEnabled();
    v.resendOnConnect = m_router->config(Model::D05).resendOnConnect;
    return v;
}

// Applies a configuration through the router. Moving from one open port to another is a connect
// (resend-on-connect) although the link only signals a change of connected state.
bool MainWindow::configureDevice(Model m, const DeviceRouter::DeviceConfig &cfg, Model *conflicting)
{
    const MidiLink *l = m_router->link(m);
    const bool was = l->isConnected();
    const QString before = l->connectedPortName();
    Model other = Model::D05;
    if (!m_router->configure(m, cfg, &other)) {
        if (conflicting)
            *conflicting = other;
        showMessage(QStringLiteral("%1 not changed: channel %2 on that port is used by the %3")
                        .arg(modelInfo(m).name).arg(cfg.channel).arg(modelInfo(other).name));
        return false;
    }
    if (was && l->isConnected() && l->connectedPortName() != before)
        noteConnected(m);
    return true;
}

// Order matters for resend-on-connect: the router stores the configuration (the flag, the channel)
// before it opens the output, so the connect that this call causes resends on the new channel.
bool MainWindow::applyDevice(Model m, const AppSettings::DeviceValues &v)
{
    m_rt[int(m)].resendBlocked = false;  // the settings changed
    const bool ok = configureDevice(m, configFor(v));
    if (ok)
        m_rt[int(m)].inputLabel = MidiLink::portKey(v.inputPort);
    ensureCurrentDevice();
    refreshConnection();
    return ok;
}

void MainWindow::applyGlobal(bool autoSend, bool followSwitches)
{
    setAutoSend(autoSend, false);
    m_router->setFollowSwitchesDevice(followSwitches);
}

void MainWindow::applySettings(const AppSettings::Values &v)
{
    setAutoSend(v.autoSend, false);
    AppSettings::DeviceValues d;
    d.enabled = true;  // version 1: the D-05 is the device
    d.outputPort = v.outputPort;
    d.inputPort = v.inputPort;
    d.channel = v.channel;
    d.follow = v.follow;
    d.resendOnConnect = v.resendOnConnect;
    applyDevice(Model::D05, d);
}

void MainWindow::setAutoSend(bool on)
{
    setAutoSend(on, true);
}

void MainWindow::setAutoSend(bool on, bool persist)
{
    if (on == m_autoSendOn)
        return;
    m_autoSendOn = on;
    {
        QSignalBlocker block(m_autoSend);  // the toggle may be what called us
        m_autoSend->setChecked(on);
    }
    if (persist && m_settings) {
        m_settings->setAutoSend(on);
        noteSettingsWrite();
    }
}

void MainWindow::openSettings()
{
    if (!m_settings) {
        notAvailable(QStringLiteral("Settings"));
        return;
    }
    if (m_settingsDialog) {
        m_settingsDialog->raise();
        m_settingsDialog->activateWindow();
        return;
    }
    m_settingsOpen = true;
    // The dialog starts from the saved values, never from the run's command-line overrides, so
    // OK cannot persist an override by accident. Which devices are on is the live state (a device a
    // command-line option turned on for this run is on). The snapshot is the live configuration,
    // for Cancel.
    SettingsDialog::State state;
    for (Model m : allModels()) {
        m_sess[int(m)] = SessionDevice();
        m_sess[int(m)].snapshot = m_router->config(m);
        AppSettings::DeviceValues saved = m_settings->device(m);
        saved.enabled = m_router->isEnabled(m);
        state.devices[int(m)] = saved;
    }
    state.autoSend = m_settings->autoSend();
    state.followSwitches = m_settings->followSwitchesDevice();
    auto *dialog = new SettingsDialog(*m_router, state, currentDevice(), this, m_systemLookup);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &SettingsDialog::testRequested, this, &MainWindow::onSettingsTest);
    connect(dialog, &QDialog::accepted, this, [this, dialog] { onSettingsAccepted(dialog); });
    connect(dialog, &QDialog::rejected, this, &MainWindow::onSettingsRejected);
    m_settingsDialog = dialog;
    dialog->open();
}

// TEST sends the device's current patch with the dialog's pending port and channel. That needs the
// link to use them, so the live port and channel change here; Cancel puts them back. A device that
// has no patch yet tests with the first one of its catalogue (the JU-06A on its believed engine).
void MainWindow::onSettingsTest(Model model, const AppSettings::DeviceValues &v)
{
    SessionDevice &sess = m_sess[int(model)];
    DeviceRouter::DeviceConfig cfg = m_router->config(model);
    cfg.enabled = true;
    cfg.channel = v.channel;
    cfg.outputPort = v.outputPort;
    sess.trialApplied = true;
    Model other = Model::D05;
    if (!configureDevice(model, cfg, &other)) {
        // The refusal is told in the dialog too: the status bar is behind the modal window.
        if (m_settingsDialog)
            m_settingsDialog->showTestRefused(model, other);
        return;
    }
    const MidiLink *l = m_router->link(model);
    sess.testedTarget = l->connectedPortName() + QLatin1Char('/') + QString::number(v.channel);
    sess.testedGeneration = m_rt[int(model)].connGeneration;  // any disconnect or reconnect after this voids it
    refreshConnection();
    if (model == Model::D05) {
        sendQuiet(model);
        return;
    }
    std::optional<PatchAddress> a = model == Model::JU06A ? m_router->ju06aTracker()->current() : l->current();
    if (model == Model::JU06A && a && a->engine() != m_router->ju06aTracker()->engine())
        a.reset();  // the unit's other engine: not a patch TEST may arm a prompt for
    if (!a)
        a = PatchAddress::fromBankNumber(model, 1, 1, model == Model::JU06A ? m_router->ju06aTracker()->engine() : 0);
    const QScopedValueRollback<bool> quiet(m_quietSend, true);  // TEST is not a selection
    if (a && !m_router->select(*a))
        showMessage(QStringLiteral("Not sent: ") + l->status());
}

void MainWindow::onSettingsAccepted(SettingsDialog *dialog)
{
    const SettingsDialog::State st = dialog->state();
    applyGlobal(st.autoSend, st.followSwitches);

    // All three at once: a swap of channels between two devices is not refused for the moment in
    // between. The dialog's OK is disabled while a conflict exists, so a refusal here is a race.
    DeviceRouter::Configs cfgs;
    for (Model m : allModels())
        cfgs[int(m)] = configFor(st.devices[int(m)]);
    std::array<bool, 3> was{};
    std::array<QString, 3> before;
    for (Model m : allModels()) {
        was[int(m)] = m_router->link(m)->isConnected();
        before[int(m)] = m_router->link(m)->connectedPortName();
    }
    Model a = Model::D05, b = Model::D05;
    if (m_router->configureAll(cfgs, &a, &b)) {
        for (Model m : allModels()) {
            m_rt[int(m)].resendBlocked = false;  // the settings changed
            m_rt[int(m)].inputLabel = MidiLink::portKey(st.devices[int(m)].inputPort);
            const MidiLink *l = m_router->link(m);
            if (was[int(m)] && l->isConnected() && l->connectedPortName() != before[int(m)])
                noteConnected(m);  // from one open port to another is a connect too
        }
    } else {
        showMessage(QStringLiteral("Settings not applied: the %1 and the %2 share a port and a channel")
                        .arg(modelInfo(a).name, modelInfo(b).name));
    }

    for (Model m : allModels()) {
        AppSettings::DeviceValues toSave = st.devices[int(m)];
        if (!m_persistPorts) {
            const AppSettings::DeviceValues saved = m_settings->device(m);
            toSave.outputPort = saved.outputPort;
            toSave.inputPort = saved.inputPort;
        }
        m_settings->setDevice(m, toSave);
        m_runOnly[int(m)] = false;  // saved now
    }
    m_settings->setAutoSend(st.autoSend);
    m_settings->setFollowSwitchesDevice(st.followSwitches);
    m_settingsOpen = false;
    for (Model m : allModels()) {
        SessionDevice &sess = m_sess[int(m)];
        const MidiLink *l = m_router->link(m);
        const QString target = l->connectedPortName() + QLatin1Char('/') + QString::number(st.devices[int(m)].channel);
        // A TEST to this very port and channel has just put the patch on the synth.
        const bool justTested = dialog->testPressed(m) && target == sess.testedTarget
                                && m_rt[int(m)].connGeneration == sess.testedGeneration;
        if (sess.connectPending && m_router->config(m).resendOnConnect && l->isConnected() && !justTested)
            resendCurrent(m);
        sess.connectPending = false;
        sess.trialApplied = false;
    }
    ensureCurrentDevice();
    refreshConnection();
    noteSettingsWrite();
}

void MainWindow::onSettingsRejected()
{
    // All snapshots at once, as OK does: restoring one by one can be refused by another device's
    // trial configuration that is still in place. A device without a trial keeps its live one.
    bool any = false;
    DeviceRouter::Configs cfgs;
    std::array<bool, 3> was{};
    std::array<QString, 3> before;
    for (Model m : allModels()) {
        SessionDevice &sess = m_sess[int(m)];
        any = any || sess.trialApplied;
        cfgs[int(m)] = sess.trialApplied ? sess.snapshot : m_router->config(m);
        was[int(m)] = m_router->link(m)->isConnected();
        before[int(m)] = m_router->link(m)->connectedPortName();
    }
    if (any) {
        Model a = Model::D05, b = Model::D05;
        if (m_router->configureAll(cfgs, &a, &b)) {
            for (Model m : allModels()) {
                const MidiLink *l = m_router->link(m);
                if (was[int(m)] && l->isConnected() && l->connectedPortName() != before[int(m)])
                    noteConnected(m);
            }
        } else {
            showMessage(QStringLiteral("Settings not restored: the %1 and the %2 share a port and a channel")
                            .arg(modelInfo(a).name, modelInfo(b).name));
        }
    }
    for (Model m : allModels())
        m_sess[int(m)].trialApplied = false;
    m_settingsOpen = false;
    refreshConnection();
    // A connect that happened while the dialog was open (or the restore itself) still counts,
    // under the resend setting that was in force when it opened.
    for (Model m : allModels()) {
        SessionDevice &sess = m_sess[int(m)];
        if (sess.connectPending && sess.snapshot.resendOnConnect && m_router->link(m)->isConnected())
            resendCurrent(m);
        sess.connectPending = false;
    }
}

// On quit: the geometry, the patch of each device to come back to and the JU-06A's engine state.
// Not written per send.
void MainWindow::saveStateNow()
{
    if (!m_settings)
        return;
    // Command-line overrides are never persisted (Cli::startup): a device that a flag turned on for
    // this run only (markRunOnly) writes nothing, the saved engine stays unless
    // it changed during the run, and the current device is written when the user switches (see
    // onDeviceChanged), not here.
    auto savedOn = [this](Model m) { return !m_runOnly[int(m)]; };
    if (savedOn(Model::D05))
        m_settings->setLastPatch(Model::D05, m_current);
    for (Model m : {Model::JP08, Model::JU06A}) {
        // The JU-06A's patch is the tracker's (none after a flip into an engine that shows ---).
        const std::optional<PatchAddress> cur = m == Model::JU06A ? currentOf(m) : m_router->link(m)->current();
        if (savedOn(m) && cur)
            m_settings->setLastPatch(m, *cur);
    }
    if (savedOn(Model::JU06A)) {
        const EngineTracker *t = m_router->ju06aTracker();
        if (m_engineChanged)
            m_settings->setEngine(t->engine());
        for (int e : {60, 106}) {
            const int r = t->remembered(e);
            m_settings->setRemembered(e, r >= 0 ? std::optional<int>(r) : std::nullopt);
        }
    }
    m_settings->setWindowGeometry(saveGeometry());
    noteSettingsWrite();
}

// A failed settings write is reported once in the status bar, not on every attempt.
void MainWindow::noteSettingsWrite()
{
    if (m_settings && m_settings->writeFailed() && !m_writeReported) {
        m_writeReported = true;
        showMessage(QStringLiteral("Settings could not be saved"));
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    finishEditing();
    flushPendingRename();
    saveStateNow();
    QMainWindow::closeEvent(event);
}

}  // namespace d05
