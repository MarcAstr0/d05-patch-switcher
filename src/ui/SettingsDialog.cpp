#include "ui/SettingsDialog.h"

#include "midi/MidiBackend.h"
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

}  // namespace

SettingsDialog::SettingsDialog(MidiBackend &backend, const MidiLink &link, const AppSettings::Values &initial,
                               QWidget *parent)
    : QDialog(parent)
    , m_backend(backend)
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
    // 26 tall like the combo (DESIGN 10.1); the shared dialog-button rule makes buttons 28.
    m_refresh->setStyleSheet(QStringLiteral("QPushButton { min-height: 24px; max-height: 24px; }"
                                            "QPushButton:focus { min-height: 22px; max-height: 22px; }"));
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

    // Tab order follows the reading order.
    QWidget *order[] = {m_outCombo, m_refresh, m_channel, m_inCombo, m_follow, m_autoSend, m_resend,
                        m_test, m_cancel, m_ok};
    for (size_t i = 0; i + 1 < sizeof(order) / sizeof(order[0]); ++i)
        setTabOrder(order[i], order[i + 1]);

    connect(m_ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_refresh, &QPushButton::clicked, this, &SettingsDialog::refreshPorts);
    connect(m_outCombo, &QComboBox::currentIndexChanged, this, [this] { updateStatus(); });
    connect(m_inCombo, &QComboBox::currentIndexChanged, this, [this] { updateEnabled(); });
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
        combo->addItem(MidiLink::portKey(wanted), wanted);
        index = combo->count() - 1;
    }
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

void SettingsDialog::refreshPorts()
{
    // Re-enumerates; whatever is selected stays selected (or stays as a "missing" entry).
    const auto selected = [](const QComboBox *c, const QString &fallback) {
        return c->count() > 0 ? c->currentData().toString() : fallback;
    };
    const QString wantOut = m_outCombo->count() > 0 ? selected(m_outCombo, QString()) : m_initial.outputPort;
    const QString wantIn = m_inCombo->count() > 0 ? selected(m_inCombo, QString()) : m_initial.inputPort;
    QString outErr, inErr;
    const QStringList outs = m_backend.outputPortNames(&outErr);
    const QStringList ins = m_backend.inputPortNames(&inErr);
    m_listError = outErr;
    m_outNames = outs;
    fillCombo(m_outCombo, outs, wantOut, false);
    fillCombo(m_inCombo, ins, wantIn, true);
    updateStatus();
    updateEnabled();
}

void SettingsDialog::updateEnabled()
{
    m_follow->setEnabled(!m_inCombo->currentData().toString().isEmpty());
}

AppSettings::Values SettingsDialog::values() const
{
    AppSettings::Values v;
    v.outputPort = MidiLink::portKey(m_outCombo->currentData().toString());
    v.inputPort = MidiLink::portKey(m_inCombo->currentData().toString());
    v.channel = m_channel->value();
    v.autoSend = m_autoSend->isChecked();
    v.follow = m_follow->isChecked();
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
    const QString key = MidiLink::portKey(selected);
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
    } else if (m_link.status().startsWith(QStringLiteral("Error")) && !m_link.outputPortName().isEmpty()
               && MidiLink::portKey(m_link.outputPortName()) == key) {
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
