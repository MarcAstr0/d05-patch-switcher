#include "ui/MainWindow.h"

#include "app/Paths.h"
#include "core/Library.h"
#include "core/PatchCatalog.h"
#include "core/SearchQuery.h"
#include "midi/MidiLink.h"
#include "ui/BankGrid.h"
#include "ui/DisplayWidget.h"
#include "ui/GroupBar.h"
#include "ui/Nameplate.h"
#include "ui/PanelButton.h"
#include "ui/PatchDelegate.h"
#include "ui/PatchListView.h"
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
        p.fillRect(rect(), m_lit ? Theme::ledOn : Theme::ledOff);
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

MainWindow::MainWindow(PatchCatalog &catalog, Library &library, MidiLink &link, QWidget *parent)
    : QMainWindow(parent)
    , m_catalog(catalog)
    , m_library(library)
    , m_link(link)
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

    buildWidgets();
    buildActions();
    resize(960, 560);
    layoutAll();

    applyCurrent(m_current);
    updateDisplayConnection();
    refreshConnection();
    if (!m_startupError.isEmpty())
        showMessage(m_startupError);
    onModelReset();
    highlightCurrent(true);
    m_search->setFocus();
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

    m_nameplate = new Nameplate(Paths::logoPath(), m_central);
    m_display = new DisplayWidget(m_central);
    m_groupBar = new GroupBar(m_central);
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

    m_search = new QLineEdit(m_central);
    m_search->setObjectName(QStringLiteral("searchField"));
    m_search->setPlaceholderText(QStringLiteral("Search names or addresses: brass, P3-45"));
    m_search->setClearButtonEnabled(false);
    m_search->setFont(Theme::font(Theme::Role::Field));
    m_search->setAttribute(Qt::WA_MacShowFocusRect, false);
    m_search->installEventFilter(this);

    m_model = new PatchListModel(m_catalog, m_library, this);
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
                        m_system, m_groupBar, m_grid, m_prev, m_next};
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
    connect(&m_library, &Library::userNamesChanged, this, &MainWindow::refreshDisplayPatch);
    connect(m_list, &PatchListView::rowClicked, this, [this](const QModelIndex &index) {
        sendAddress(m_model->addressAt(index));
    });
    connect(m_model, &QAbstractItemModel::modelAboutToBeReset, this, &MainWindow::onModelAboutToReset);
    connect(m_model, &QAbstractItemModel::modelReset, this, &MainWindow::onModelReset);

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

    connect(&m_link, &MidiLink::sent, this, &MainWindow::onSent);
    connect(&m_link, &MidiLink::currentChanged, this, &MainWindow::onFollowed);
    connect(&m_link, &MidiLink::connectedChanged, this, &MainWindow::onConnectedChanged);
    connect(&m_link, &MidiLink::inputStatusChanged, this, [this](const QString &status) {
        if (status.startsWith(QStringLiteral("Error")))
            showMessage(QStringLiteral("Input: ") + status);
        refreshConnection();
    });
    connect(m_display, &DisplayWidget::sizeChanged, this, [this] {
        if (!m_inLayout)
            layoutAll();  // a DPR or screen change moved the band without a window resize
    });
    connect(&m_link, &MidiLink::statusChanged, this, [this](const QString &status) {
        if (status.startsWith(QStringLiteral("Error")))
            showMessage(status);
        refreshConnection();
    });
    connect(&m_library, &Library::saveFailed, this,
            [this](const QString &text) { showMessage(QStringLiteral("Library not saved: ") + text); });
    connect(m_display, &DisplayWidget::sentChanged, this, [this](bool on) {
        static_cast<StatusStrip *>(m_statusStrip)->led->setLit(on);
    });
}

void MainWindow::buildActions()
{
    const auto make = [this](const QString &name, const QString &text, const QKeySequence &key, auto slot) {
        auto *a = new QAction(text, this);
        a->setObjectName(name);
        if (!key.isEmpty())
            a->setShortcut(key);
        a->setShortcutContext(Qt::WindowShortcut);
        connect(a, &QAction::triggered, this, slot);
        addAction(a);
        return a;
    };

    make(QStringLiteral("actionFocusSearch"), QStringLiteral("Find"), QKeySequence(Qt::Key_Slash), [this] { focusSearch(); });
    make(QStringLiteral("actionFocusSearchCtrlF"), QStringLiteral("Find"), QKeySequence(Qt::CTRL | Qt::Key_F),
         [this] { focusSearch(); });
    make(QStringLiteral("actionEscape"), QStringLiteral("Clear search"), QKeySequence(Qt::Key_Escape), [this] { onEscape(); });
    // Every trigger sends MIDI: holding the key must not stream patches at the synth.
    make(QStringLiteral("actionPrev"), QStringLiteral("Previous patch"), QKeySequence(Qt::Key_PageUp), [this] { step(-1); })
        ->setAutoRepeat(false);
    make(QStringLiteral("actionNext"), QStringLiteral("Next patch"), QKeySequence(Qt::Key_PageDown), [this] { step(1); })
        ->setAutoRepeat(false);
    make(QStringLiteral("actionGridEntry"), QStringLiteral("Enter bank and number"), QKeySequence(Qt::CTRL | Qt::Key_G),
         [this] { beginGridEntry(); });
    make(QStringLiteral("actionViewAll"), QStringLiteral("All"), QKeySequence(Qt::CTRL | Qt::Key_1),
         [this] { m_viewAll->setChecked(true); });
    make(QStringLiteral("actionViewFavourites"), QStringLiteral("Favourites"), QKeySequence(Qt::CTRL | Qt::Key_2),
         [this] { m_viewFav->setChecked(true); });
    make(QStringLiteral("actionViewRecents"), QStringLiteral("Recents"), QKeySequence(Qt::CTRL | Qt::Key_3),
         [this] { m_viewRecent->setChecked(true); });
    make(QStringLiteral("actionToggleFavourite"), QStringLiteral("Toggle favourite"), QKeySequence(Qt::CTRL | Qt::Key_D),
         [this] { toggleHighlightedFavorite(); })
        ->setAutoRepeat(false);
    make(QStringLiteral("actionRename"), QStringLiteral("Rename"), QKeySequence(Qt::Key_F2), [this] { renameHighlighted(); })
        ->setAutoRepeat(false);
    make(QStringLiteral("actionAutoSend"), QStringLiteral("Auto-send"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A),
         [this] { setAutoSend(!m_autoSendOn); })
        ->setAutoRepeat(false);

    // The system menu (DESIGN 8.2). The actions carry their shortcuts, so they are the
    // window's shortcuts for Settings, the sheet and Quit.
    m_systemMenu = new QMenu(this);
    m_systemMenu->setObjectName(QStringLiteral("systemMenu"));
    auto *settings = make(QStringLiteral("actionSettings"), QStringLiteral("Settings..."),
                          QKeySequence(Qt::CTRL | Qt::Key_Comma), [this] { openSettings(); });
    auto *sheet = make(QStringLiteral("actionShortcuts"), QStringLiteral("Keyboard Shortcuts"), QKeySequence(Qt::Key_F1),
                       [this] { notAvailable(QStringLiteral("The shortcut sheet")); });
    auto *clear = make(QStringLiteral("actionClearRecents"), QStringLiteral("Clear Recents..."), QKeySequence(),
                       [this] { confirmClearRecents(); });
    auto *about = make(QStringLiteral("actionAbout"), QStringLiteral("About D-05 Patch Switcher"), QKeySequence(),
                       [this] { notAvailable(QStringLiteral("The About box")); });
    auto *quit = make(QStringLiteral("actionQuit"), QStringLiteral("Quit"), QKeySequence(Qt::CTRL | Qt::Key_Q),
                      [this] { saveAndQuit(); });
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

void MainWindow::layoutAll()
{
    const QScopedValueRollback<bool> guard(m_inLayout, true);
    const int W = width();
    const int H = height();

    m_display->setStep(DisplayWidget::pitchForWidth(W), W);
    const QSize ds = m_display->sizeHint();
    m_display->setGeometry(kMargin, 46, ds.width(), ds.height());
    m_nameplate->setGeometry(kMargin, 10, qMin(Nameplate::SlotMaxWidth, W - 2 * kMargin), Nameplate::SlotHeight);

    const int B = 46 + ds.height() + 14;
    m_bandTop = B;
    m_band->setGeometry(0, B, W, H - kStatusHeight - B);
    m_band->lower();

    // Left column (DESIGN 4.3): faces at x = 16 + 50 col.
    m_groupBar->move(kMargin - 4, B + 12);
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
    const QColor &primary = Theme::labelPrimary;
    band->setLabels({
        {QStringLiteral("ALL"), facesX[0] + 22, 15 + 9, primary},
        {QStringLiteral("FAVOURITES"), facesX[1] + 22, 15 + 9, primary},
        {QStringLiteral("RECENTS"), facesX[2] + 22, 15 + 9, primary},
        {QStringLiteral("AUTO SEND"), facesX[3] + 22, 15 + 9, Theme::labelRed},
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
    if (e->type() == QEvent::WindowActivate)
        m_link.checkConnection();  // FR-10: the only check while connected (no timer)
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
    if (!m_link.sendPatch(address)) {
        showMessage(QStringLiteral("Not sent: ") + m_link.status());
        m_grid->setCurrent(m_current);  // end any entry, back to the current patch
        return false;
    }
    return true;
}

void MainWindow::sendHighlighted()
{
    const auto a = highlighted();
    if (a)
        sendAddress(*a);
}

void MainWindow::step(int direction)
{
    sendAddress(direction < 0 ? PatchCatalog::prev(m_current) : PatchCatalog::next(m_current));
}

void MainWindow::beginGridEntry()
{
    m_grid->setFocus(Qt::ShortcutFocusReason);
    m_grid->beginEntry(m_current.group());
}

void MainWindow::onSent(const PatchAddress &address)
{
    // A resend or a TEST is not a selection: no Recents entry, and it leaves an open rename
    // editor and the highlight alone.
    if (!m_quietSend) {
        finishEditing();
        m_library.recordSend(address);
    }
    m_resendBlocked = false;  // a send worked: resend-on-connect may try again
    applyCurrent(address, !m_quietSend);
    m_display->flashSent();
}

// An incoming Bank Select / Program Change moved the current patch (follow, FR-11): every
// view follows, the activity LED blinks, and nothing is sent, so auto-send cannot fire.
void MainWindow::onFollowed(const PatchAddress &address)
{
    finishEditing();
    applyCurrent(address);
    m_display->flashActivity();
}

// The current patch changed (sent, followed or restored): every view of it follows.
void MainWindow::applyCurrent(const PatchAddress &address, bool moveHighlight)
{
    m_current = address;
    m_model->setCurrent(address);
    m_display->setPatch(address, m_catalog.name(address), m_catalog.mode(address));
    m_groupBar->setLitGroup(address.group());
    m_grid->setCurrent(address);
    if (moveHighlight)
        highlightCurrent(false);
}

void MainWindow::setCurrent(const PatchAddress &address)
{
    m_link.setCurrent(address);
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
    if (!SearchQuery(text).isEmpty()) {
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
    m_savedHighlight = highlighted();
    if (m_savedHighlight)
        m_lastHighlight = m_savedHighlight;
}

void MainWindow::onModelReset()
{
    m_display->setSearch(m_model->searchActive(), m_model->rowCount());
    m_list->setEmptyText(m_model->emptyTitle(), m_model->emptyNote());
    if (m_model->rowCount() == 0)
        return;
    switch (m_restore) {
    case Restore::First:
        highlightRow(0, false);
        m_list->verticalScrollBar()->setValue(0);
        break;
    case Restore::Highlight:
        // Switching views keeps the highlighted patch if the new view has it.
        if (m_savedHighlight && m_model->rowOf(*m_savedHighlight) >= 0)
            highlightRow(m_model->rowOf(*m_savedHighlight), true);
        else if (m_model->rowOf(m_current) >= 0)
            highlightCurrent(true);
        else
            highlightRow(0, false);
        break;
    case Restore::BeforeSearch:
        // The search was cleared: back to the row the user had, else the current patch.
        if (m_preSearch && m_model->rowOf(*m_preSearch) >= 0)
            highlightRow(m_model->rowOf(*m_preSearch), true);
        else if (m_model->rowOf(m_current) >= 0)
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
    const int row = m_model->rowOf(m_current);
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
    if (!a)
        a = m_lastHighlight ? m_lastHighlight : std::optional<PatchAddress>(m_current);
    toggleFavorite(*a);
}

void MainWindow::renameHighlighted()
{
    const QModelIndex i = m_list->currentIndex();
    if (!i.isValid())
        return;
    if (!m_model->addressAt(i).isUser()) {
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
        m_library.clearRecents();
        box->accept();
    });
    connect(cancel, &QAbstractButton::clicked, box, &QDialog::reject);
    cancel->setFocus();  // initial focus is the safe choice
    m_clearBox = box;
    box->open();
}

void MainWindow::focusSearch()
{
    m_search->setFocus(Qt::ShortcutFocusReason);
    m_search->selectAll();
}

void MainWindow::onEscape()
{
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
    m_display->setEntry(m_grid->entryText());
    m_groupBar->setLitGroup(idle ? m_current.group() : m_grid->entryGroup(), idle);
    if (!idle && !m_grid->hasFocus())
        m_grid->setFocus(Qt::OtherFocusReason);
}

// ---------------------------------------------------------------------------- status

void MainWindow::updateDisplayConnection()
{
    m_display->setConnected(m_link.isConnected());
}

void MainWindow::refreshConnection()
{
    auto *s = static_cast<StatusStrip *>(m_statusStrip);
    const bool connected = m_link.isConnected();
    const QString out = connected ? m_link.connectedPortKey() : m_link.outputPortName();
    const QFontMetrics fm(Theme::font(Theme::Role::StatusValue));
    StatusStrip::setValue(s->port, out.isEmpty() ? QStringLiteral("None") : fm.elidedText(out, Qt::ElideMiddle, 280),
                          out.isEmpty());
    s->port->setToolTip(out);
    s->notFound->setVisible(!connected && !out.isEmpty());
    s->chValue->setText(QString::number(m_link.channel()));
    StatusStrip::setValue(s->inValue,
                          m_inputLabel.isEmpty() ? QStringLiteral("None") : fm.elidedText(m_inputLabel, Qt::ElideMiddle, 280),
                          m_inputLabel.isEmpty());
    // FOLLOW only while the input is really open; follow on with an input that is missing or
    // failed shows NOT FOUND after it, as the output does.
    const bool wantsInput = m_link.followEnabled() && !m_inputLabel.isEmpty();
    s->follow->setVisible(wantsInput && m_link.inputConnected());
    s->inNotFound->setVisible(wantsInput && !m_link.inputConnected());
    updateDisplayConnection();
    relayoutStatus();
}

void MainWindow::setInputPortLabel(const QString &name)
{
    m_inputLabel = name;
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

void MainWindow::onConnectedChanged(bool connected)
{
    updateDisplayConnection();
    refreshConnection();
    if (connected)
        noteConnected();
    else
        ++m_connGeneration;
}

void MainWindow::noteConnected()
{
    ++m_connGeneration;
    if (m_settingsOpen)
        m_connectPending = true;  // decided when the dialog closes
    else if (m_resendOnConnect)
        resendCurrent();
}

// Opens `port` as the output. MidiLink only signals a change of connected state, but moving
// from one open port to another is a connect too (resend-on-connect), so it is noted here.
void MainWindow::setOutput(const QString &port)
{
    const bool was = m_link.isConnected();
    const QString before = MidiLink::portKey(m_link.connectedPortName());
    m_link.setOutputPort(port);
    if (was && m_link.isConnected() && MidiLink::portKey(m_link.connectedPortName()) != before)
        noteConnected();
}

// Resend-on-connect: the output (re)connected, so tell the synth what the app shows, once.
// A failed resend is not retried on every poll reconnect: it stays blocked until a send works
// or the settings change.
void MainWindow::resendCurrent()
{
    if (m_resendBlocked)
        return;
    if (!sendQuiet(m_current))
        m_resendBlocked = true;
}

bool MainWindow::sendQuiet(const PatchAddress &address)
{
    m_quietSend = true;
    const bool ok = sendAddress(address);
    m_quietSend = false;
    return ok;
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

AppSettings::Values MainWindow::currentSettings() const
{
    AppSettings::Values v;
    v.outputPort = m_link.isConnected() ? m_link.connectedPortKey() : MidiLink::portKey(m_link.outputPortName());
    v.inputPort = m_inputLabel;
    v.channel = m_link.channel();
    v.autoSend = m_autoSendOn;
    v.follow = m_link.followEnabled();
    v.resendOnConnect = m_resendOnConnect;
    return v;
}

bool MainWindow::sameOutput(const QString &port) const
{
    return m_link.isConnected() && !port.isEmpty()
           && MidiLink::portKey(m_link.connectedPortName()).compare(MidiLink::portKey(port), Qt::CaseInsensitive) == 0;
}

// Order matters for resend-on-connect: the flag and the channel are in place before the
// output opens, so the connect that this call causes resends on the new channel.
void MainWindow::applySettings(const AppSettings::Values &v)
{
    m_resendOnConnect = v.resendOnConnect;
    m_resendBlocked = false;  // the settings changed
    setAutoSend(v.autoSend, false);
    if (v.channel != m_link.channel())
        m_link.setChannel(v.channel);
    m_inputLabel = MidiLink::portKey(v.inputPort);
    // Reopen the input only when it actually changed.
    if (v.inputPort != m_inputName) {
        m_inputName = v.inputPort;
        m_link.setInputPort(v.inputPort);
    }
    if (v.follow != m_link.followEnabled())
        m_link.setFollowEnabled(v.follow);
    if (!sameOutput(v.outputPort))
        setOutput(v.outputPort);
    refreshConnection();
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
    m_snapshot = currentSettings();
    m_settingsOpen = true;
    m_trialApplied = false;
    m_connectPending = false;
    m_testedTarget.clear();
    // The dialog starts from the saved values, never from the run's command-line overrides, so
    // OK cannot persist an override by accident. The snapshot is the live state, for Cancel.
    auto *dialog = new SettingsDialog(m_link, m_settings->values(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &SettingsDialog::testRequested, this, &MainWindow::onSettingsTest);
    connect(dialog, &QDialog::accepted, this, [this, dialog] { onSettingsAccepted(dialog); });
    connect(dialog, &QDialog::rejected, this, &MainWindow::onSettingsRejected);
    m_settingsDialog = dialog;
    dialog->open();
}

// TEST sends the current patch with the dialog's pending port and channel. That needs the
// link to use them, so the live port and channel change here; Cancel puts them back.
void MainWindow::onSettingsTest(const AppSettings::Values &v)
{
    m_trialApplied = true;
    m_link.setChannel(v.channel);
    if (!sameOutput(v.outputPort))
        setOutput(v.outputPort);
    m_testedTarget = MidiLink::portKey(m_link.connectedPortName()) + QLatin1Char('/') + QString::number(v.channel);
    m_testedGeneration = m_connGeneration;  // any disconnect or reconnect after this voids it
    refreshConnection();
    sendQuiet(m_current);
}

void MainWindow::onSettingsAccepted(SettingsDialog *dialog)
{
    const AppSettings::Values v = dialog->values();
    const bool tested = dialog->testPressed();
    applySettings(v);  // the dialog is still counted as open: a connect it causes is only noted
    m_settings->setValues(v);
    m_settingsOpen = false;
    const QString target = MidiLink::portKey(m_link.connectedPortName()) + QLatin1Char('/') + QString::number(v.channel);
    // A TEST to this very port and channel has just put the patch on the synth.
    const bool justTested = tested && target == m_testedTarget && m_connGeneration == m_testedGeneration;
    if (m_connectPending && m_resendOnConnect && m_link.isConnected() && !justTested)
        resendCurrent();
    m_connectPending = false;
    noteSettingsWrite();
}

void MainWindow::onSettingsRejected()
{
    if (m_trialApplied) {
        m_link.setChannel(m_snapshot.channel);
        if (!sameOutput(m_snapshot.outputPort))
            setOutput(m_snapshot.outputPort);
        refreshConnection();
    }
    m_trialApplied = false;
    m_settingsOpen = false;
    // A connect that happened while the dialog was open (or the restore itself) still counts,
    // under the resend setting that was in force when it opened.
    if (m_connectPending && m_snapshot.resendOnConnect && m_link.isConnected())
        resendCurrent();
    m_connectPending = false;
}

// On quit: the geometry and the patch to come back to. Not written per send.
void MainWindow::saveStateNow()
{
    if (!m_settings)
        return;
    m_settings->setLastPatch(m_current);
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
