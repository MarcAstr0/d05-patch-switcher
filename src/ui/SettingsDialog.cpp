#include "ui/SettingsDialog.h"

#include "midi/MidiLink.h"
#include "ui/SectionRule.h"
#include "ui/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFontMetrics>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QStyle>
#include <QStyledItemDelegate>

namespace d05 {

namespace {

constexpr int kMargin = 20;
constexpr int kContentW = 480;
constexpr int kControlX = 112;
constexpr int kStatusElideW = 350;
constexpr int kIsKeyRole = Qt::UserRole + 1;  // item data: true when the data is a saved key, not an OS name

// The 10 x 4 LED beside the connection text: lit (no core) when connected, else off.
class StatusLed : public QWidget {
public:
    explicit StatusLed(QWidget *parent)
        : QWidget(parent)
    {
        setFixedSize(10, 4);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    void setLit(bool on)
    {
        if (on != m_lit) {
            m_lit = on;
            update();
        }
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), m_lit ? Theme::ledOn : Theme::ledOff);
    }

private:
    bool m_lit = false;
};

// The selected port as a key: OS names are stripped, a saved key is kept as it is.
QString selectedKey(const QComboBox *c)
{
    const QString data = c->currentData().toString();
    return c->currentData(kIsKeyRole).toBool() ? data : MidiLink::portKey(data);
}

}  // namespace

SettingsDialog::SettingsDialog(const MidiLink &link, const AppSettings::Values &initial, QWidget *parent)
    : QDialog(parent)
    , m_link(link)
    , m_initial(initial)
{
    setObjectName(QStringLiteral("settingsDialog"));
    setWindowTitle(QStringLiteral("Settings"));
    setModal(true);
    setFixedSize(ClientWidth, ClientHeight);
    buildUi();

    m_channel->setValue(initial.channel);
    m_follow->setChecked(initial.follow);
    m_autoSend->setChecked(initial.autoSend);
    m_resend->setChecked(initial.resendOnConnect);
    refreshPorts();
    m_outCombo->setFocus();  // DESIGN 10.1: initial focus
}

void SettingsDialog::buildUi()
{
    // Everything is placed at the design's coordinates: content origin (20, 20).
    auto at = [](int x, int y) { return QPoint(kMargin + x, kMargin + y); };
    auto rule = [&](const QString &title, int y) {
        auto *r = new SectionRule(title, false, this);
        r->setGeometry(QRect(at(0, y), QSize(kContentW, SectionRule::Height)));
    };
    auto formLabel = [&](const QString &text, int y) {
        auto *l = new QLabel(text, this);
        l->setProperty("role", QStringLiteral("formLabel"));
        l->setFont(Theme::font(Theme::Role::PanelLabel));
        l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        l->setGeometry(QRect(at(0, y), QSize(100, 26)));  // right edge at content x 100
    };
    auto itemDelegate = [](QComboBox *c) { c->setItemDelegate(new QStyledItemDelegate(c)); };

    rule(QStringLiteral("MIDI OUTPUT"), 0);
    formLabel(QStringLiteral("PORT"), 20);
    m_outCombo = new QComboBox(this);
    m_outCombo->setObjectName(QStringLiteral("outputCombo"));
    itemDelegate(m_outCombo);
    m_outCombo->setGeometry(QRect(at(kControlX, 20), QSize(260, 26)));
    m_refresh = new QPushButton(QStringLiteral("REFRESH"), this);
    m_refresh->setObjectName(QStringLiteral("refreshButton"));
    m_refresh->setAutoDefault(false);
    // 26 tall like the combo: the QSS rule on #refreshButton sets the height (DESIGN 10.1);
    // the fixed height is kept so the size is right even before the style sheet applies.
    m_refresh->setFixedHeight(26);
    m_refresh->setGeometry(QRect(at(380, 20), QSize(100, 26)));

    formLabel(QStringLiteral("CHANNEL"), 54);
    m_channel = new QSpinBox(this);
    m_channel->setObjectName(QStringLiteral("channelSpin"));
    m_channel->setRange(1, 16);
    m_channel->setGeometry(QRect(at(kControlX, 54), QSize(72, 26)));

    m_led = new StatusLed(this);
    m_led->move(at(kControlX, 88 + 8));
    m_connectionText = new QLabel(this);
    m_connectionText->setObjectName(QStringLiteral("connectionText"));
    m_connectionText->setFont(Theme::font(Theme::Role::Body));
    m_connectionText->setProperty("state", QStringLiteral("error"));
    m_connectionText->setGeometry(QRect(at(130, 88), QSize(kStatusElideW + 8, 20)));

    rule(QStringLiteral("MIDI INPUT"), 124);
    formLabel(QStringLiteral("PORT"), 144);
    m_inCombo = new QComboBox(this);
    m_inCombo->setObjectName(QStringLiteral("inputCombo"));
    itemDelegate(m_inCombo);
    m_inCombo->setGeometry(QRect(at(kControlX, 144), QSize(260, 26)));
    // DESIGN 10.1 has no slot for an input error; this small red line sits right of the combo,
    // where the output row has REFRESH.
    m_inputStatus = new QLabel(this);
    m_inputStatus->setObjectName(QStringLiteral("inputStatusText"));
    m_inputStatus->setFont(Theme::font(Theme::Role::Note));
    m_inputStatus->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(Theme::labelRed.name()));
    m_inputStatus->setGeometry(QRect(at(380, 144), QSize(100, 26)));
    m_follow = new QCheckBox(QStringLiteral("Follow the panel"), this);
    m_follow->setObjectName(QStringLiteral("followCheck"));
    m_follow->setGeometry(QRect(at(kControlX, 178), QSize(kContentW - kControlX, 20)));

    rule(QStringLiteral("BEHAVIOUR"), 214);
    m_autoSend = new QCheckBox(QStringLiteral("Auto-send on highlight"), this);
    m_autoSend->setObjectName(QStringLiteral("autoSendCheck"));
    m_autoSend->setGeometry(QRect(at(kControlX, 234), QSize(kContentW - kControlX, 20)));
    m_resend = new QCheckBox(QStringLiteral("Resend the current patch on connect"), this);
    m_resend->setObjectName(QStringLiteral("resendCheck"));
    m_resend->setGeometry(QRect(at(kControlX, 262), QSize(kContentW - kControlX, 20)));

    rule(QStringLiteral("ON THE D-05"), 298);
    auto *note = new QLabel(
        QStringLiteral("The D-05 changes patch only when SYSTEM > MIDI > Prog.C is ON. If your unit also has a "
                       "Bank.S switch, turn it ON too."),
        this);
    note->setProperty("role", QStringLiteral("note"));
    note->setFont(Theme::font(Theme::Role::Note));
    note->setWordWrap(true);
    note->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    note->setGeometry(QRect(at(0, 318), QSize(kContentW, 30)));

    constexpr int buttonY = 368;
    m_test = new QPushButton(QStringLiteral("TEST"), this);
    m_test->setObjectName(QStringLiteral("testButton"));
    m_test->setAutoDefault(false);
    m_test->setGeometry(QRect(at(0, buttonY), QSize(100, 28)));
    m_cancel = new QPushButton(QStringLiteral("CANCEL"), this);
    m_cancel->setObjectName(QStringLiteral("cancelButton"));
    m_cancel->setAutoDefault(false);
    m_cancel->setGeometry(QRect(at(kContentW - 208, buttonY), QSize(100, 28)));
    m_ok = new QPushButton(QStringLiteral("OK"), this);
    m_ok->setObjectName(QStringLiteral("okButton"));
    m_ok->setDefault(true);
    m_ok->setGeometry(QRect(at(kContentW - 100, buttonY), QSize(100, 28)));

    Theme::applyDialogButtonFonts(this);

    // Tab order follows the reading order.
    QWidget *order[] = {m_outCombo, m_refresh, m_channel, m_inCombo, m_follow, m_autoSend, m_resend,
                        m_test, m_cancel, m_ok};
    for (size_t i = 0; i + 1 < sizeof(order) / sizeof(order[0]); ++i)
        setTabOrder(order[i], order[i + 1]);

    connect(m_ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_refresh, &QPushButton::clicked, this, &SettingsDialog::refreshPorts);
    connect(m_outCombo, &QComboBox::currentIndexChanged, this, [this] { updateStatus(); });
    connect(m_inCombo, &QComboBox::currentIndexChanged, this, [this] { updateEnabled(); updateInputStatus(); });
    connect(&m_link, &MidiLink::inputStatusChanged, this, [this] { updateInputStatus(); });
    connect(&m_link, &MidiLink::statusChanged, this, [this] { updateStatus(); });
    connect(&m_link, &MidiLink::connectedChanged, this, [this] { updateStatus(); });
    connect(m_test, &QPushButton::clicked, this, [this] {
        m_testPressed = true;
        emit testRequested(values());
    });
}

// Item text is the port key; two ports with the same key show their full names instead.
void SettingsDialog::fillCombo(QComboBox *combo, const QStringList &names, const QString &wanted, bool withNone)
{
    QSignalBlocker block(combo);
    combo->clear();
    if (withNone)
        combo->addItem(QStringLiteral("None"), QString());
    QStringList keys;
    for (const QString &n : names)
        keys << MidiLink::portKey(n);
    for (int i = 0; i < names.size(); ++i) {
        const bool dup = keys.count(keys.at(i)) > 1;
        combo->addItem(dup ? names.at(i) : keys.at(i), names.at(i));
    }
    const QString match = MidiLink::matchPort(names, wanted);
    int index = -1;
    if (!match.isEmpty()) {
        index = combo->findData(match);
    } else if (!wanted.isEmpty()) {
        // Configured but not listed right now: keep it visible so OK does not forget it.
        combo->addItem(wanted, wanted);
        combo->setItemData(combo->count() - 1, true, kIsKeyRole);  // already a key: never strip it again
        index = combo->count() - 1;
    }
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

void SettingsDialog::refreshPorts()
{
    // Re-enumerates; whatever is selected stays selected (or stays as a "missing" entry).
    const QString wantOut = m_outCombo->count() > 0 ? selectedKey(m_outCombo) : m_initial.outputPort;
    const QString wantIn = m_inCombo->count() > 0 ? selectedKey(m_inCombo) : m_initial.inputPort;
    QString outErr, inErr;
    const QStringList outs = m_link.outputPortNames(&outErr);
    const QStringList ins = m_link.inputPortNames(&inErr);
    m_listError = outErr;
    m_outNames = outs;
    m_inListError = inErr;
    m_inNames = ins;
    fillCombo(m_outCombo, outs, wantOut, false);
    fillCombo(m_inCombo, ins, wantIn, true);
    updateStatus();
    updateInputStatus();
    updateEnabled();
}

// The input row's problem, if any: a listing error, a port that is not listed, or the link's own
// open error for the port that is wanted. Empty when the input is None or fine.
void SettingsDialog::updateInputStatus()
{
    const QString selected = m_inCombo->currentData().toString();
    QString text;
    if (!m_inListError.isEmpty()) {
        text = QStringLiteral("Error: ") + m_inListError;
    } else if (!selected.isEmpty() && MidiLink::matchPort(m_inNames, selected).isEmpty()) {
        text = QStringLiteral("Port not found");
    } else if (!selected.isEmpty() && !m_link.inputConnected() && m_link.inputStatus().startsWith(QStringLiteral("Error"))
               && !MidiLink::matchPort({selected}, m_link.inputPortName()).isEmpty()) {
        text = m_link.inputStatus();
    }
    m_inputFullStatus = text;
    const QFontMetrics fm(Theme::font(Theme::Role::Note));
    m_inputStatus->setText(fm.elidedText(text, Qt::ElideRight, 100));
    m_inputStatus->setToolTip(text);
}

void SettingsDialog::updateEnabled()
{
    m_follow->setEnabled(!m_inCombo->currentData().toString().isEmpty());
}

AppSettings::Values SettingsDialog::values() const
{
    AppSettings::Values v;
    v.outputPort = selectedKey(m_outCombo);
    v.inputPort = selectedKey(m_inCombo);
    v.channel = m_channel->value();
    v.autoSend = m_autoSend->isChecked();
    // Follow needs an input port: with None it is off whatever the (disabled) box shows.
    v.follow = m_follow->isChecked() && !v.inputPort.isEmpty();
    v.resendOnConnect = m_resend->isChecked();
    return v;
}

QString SettingsDialog::connectionState() const
{
    return m_connectionText->property("state").toString();
}

void SettingsDialog::updateStatus()
{
    const QString selected = m_outCombo->currentData().toString();
    const QString key = selectedKey(m_outCombo);
    QString text;
    bool ok = false;
    if (!m_listError.isEmpty()) {
        text = QStringLiteral("Error: ") + m_listError;
    } else if (selected.isEmpty()) {
        text = QStringLiteral("Port not found");
    } else if (m_link.isConnected() && MidiLink::portKey(m_link.connectedPortName()) == key
               && !MidiLink::matchPort(m_outNames, selected).isEmpty()) {
        text = QStringLiteral("Connected to ") + key;
        ok = true;
    } else if (!m_link.isConnected() && m_link.status().startsWith(QStringLiteral("Error"))
               && !MidiLink::matchPort({selected}, m_link.outputPortName()).isEmpty()) {
        // The wanted port (matched the way the link matches: "bout" finds "Boutique") failed.
        text = m_link.status();  // the port that is wanted failed to open: say why
    } else if (!MidiLink::matchPort(m_outNames, selected).isEmpty()) {
        // Listed: the port is there to be opened when OK applies (see the class comment).
        text = QStringLiteral("Connected to ") + key;
        ok = true;
    } else {
        text = QStringLiteral("Port not found");
    }
    m_fullStatus = text;
    const QFontMetrics fm(Theme::font(Theme::Role::Body));
    m_connectionText->setText(fm.elidedText(text, Qt::ElideRight, kStatusElideW));
    m_connectionText->setToolTip(text);
    const QString state = ok ? QStringLiteral("ok") : QStringLiteral("error");
    if (m_connectionText->property("state").toString() != state) {
        m_connectionText->setProperty("state", state);
        m_connectionText->style()->unpolish(m_connectionText);
        m_connectionText->style()->polish(m_connectionText);
    }
    static_cast<StatusLed *>(m_led)->setLit(ok);
    m_test->setEnabled(ok);  // DESIGN 10.1: TEST is disabled while disconnected
}

}  // namespace d05
