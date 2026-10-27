#include "ui/DevicePage.h"

#include "midi/MidiLink.h"
#include "ui/SectionRule.h"
#include "ui/Shortcuts.h"
#include "ui/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFontMetrics>
#include <QKeySequence>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QStyledItemDelegate>

namespace d05 {

namespace {

constexpr int kControlX = 112;
constexpr int kStatusElideW = 350;
constexpr int kConflictW = 364;
constexpr int kIsKeyRole = Qt::UserRole + 1;   // item data: true when the data is a saved key, not an OS name
constexpr int kIsFullRole = Qt::UserRole + 2;  // item data: true when the key is ambiguous, so the full OS name is kept

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
        p.fillRect(rect(), m_lit ? Theme::colors().ledOn : Theme::colors().ledOff);
    }

private:
    bool m_lit = false;
};

// The selected port as it is saved: OS names are stripped to a key, except when several ports
// share that key (two identical devices), which keep their full name; a saved key or name is
// kept as it is.
QString selectedKey(const QComboBox *c)
{
    const QString data = c->currentData().toString();
    return c->currentData(kIsKeyRole).toBool() || c->currentData(kIsFullRole).toBool() ? data : MidiLink::portKey(data);
}

QString ctrlE()
{
    return QKeySequence(Qt::CTRL | Qt::Key_E).toString(QKeySequence::NativeText);
}

QString noteFor(Model m)
{
    switch (m) {
    case Model::D05:
        return QStringLiteral("The D-05 changes patch only when SYSTEM > MIDI > Prog.C is ON. If your unit also has a "
                              "Bank.S switch, turn it ON too.");
    case Model::JP08:
        return QStringLiteral("Set the JP-08's MIDI channel with [MANUAL] + [2], then [1] to [16]. The JP-08 has no "
                              "program change switch: the channel is the only setting that matters.");
    case Model::JU06A:
        return QStringLiteral("Set the JU-06A's MIDI channel: hold ARPEGGIO [ON/OFF], press [2], turn [VALUE]. The app "
                              "cannot read the 60/106 switch. When you flip it, the app follows if it sees the flip on "
                              "the input; otherwise press %1.")
            .arg(ctrlE());
    }
    return QString();
}

}  // namespace

DevicePage::DevicePage(Model model, const MidiLink &link, QWidget *parent)
    : QWidget(parent)
    , m_model(model)
    , m_link(link)
{
    setObjectName(QStringLiteral("devicePage_") + modelInfo(model).id);
    setFixedSize(Width, Height);
    buildUi();
}

void DevicePage::buildUi()
{
    auto rule = [&](const QString &title, int y) {
        auto *r = new SectionRule(title, false, this);
        r->setGeometry(QRect(QPoint(0, y), QSize(Width, SectionRule::Height)));
    };
    auto formLabel = [&](const QString &text, int y) {
        auto *l = new QLabel(text, this);
        l->setProperty("role", QStringLiteral("formLabel"));
        l->setFont(Theme::font(Theme::Role::PanelLabel));
        l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        l->setGeometry(QRect(QPoint(0, y), QSize(100, 26)));  // right edge at page x 100
        return l;
    };
    auto itemDelegate = [](QComboBox *c) { c->setItemDelegate(new QStyledItemDelegate(c)); };
    auto useButton = [&](const char *name, int y) {
        auto *b = new QPushButton(QStringLiteral("USE"), this);
        b->setObjectName(QString::fromLatin1(name));
        b->setAutoDefault(false);
        b->setFixedHeight(26);
        b->setGeometry(QRect(QPoint(380, y), QSize(72, 26)));
        b->setEnabled(false);
        return b;
    };

    rule(QStringLiteral("MIDI OUTPUT"), 0);
    formLabel(QStringLiteral("PORT"), 20);
    m_outCombo = new QComboBox(this);
    m_outCombo->setObjectName(QStringLiteral("outputCombo"));
    itemDelegate(m_outCombo);
    m_outCombo->setGeometry(QRect(QPoint(kControlX, 20), QSize(260, 26)));
    m_useOut = useButton("useOutputButton", 20);
    m_refresh = new QPushButton(QStringLiteral("REFRESH"), this);
    m_refresh->setObjectName(QStringLiteral("refreshButton"));
    m_refresh->setAutoDefault(false);
    // 26 tall like the combo: the QSS rule on #refreshButton sets the height (DESIGN 10.1);
    // the fixed height is kept so the size is right even before the style sheet applies.
    m_refresh->setFixedHeight(26);
    m_refresh->setGeometry(QRect(QPoint(460, 20), QSize(100, 26)));

    formLabel(QStringLiteral("CHANNEL"), 54);
    m_channel = new QSpinBox(this);
    m_channel->setObjectName(QStringLiteral("channelSpin"));
    m_channel->setRange(1, 16);
    m_channel->setGeometry(QRect(QPoint(kControlX, 54), QSize(72, 26)));
    m_conflictLabel = new QLabel(this);
    m_conflictLabel->setObjectName(QStringLiteral("channelConflictText"));
    m_conflictLabel->setFont(Theme::font(Theme::Role::Note));
    m_conflictLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_conflictLabel->setGeometry(QRect(QPoint(196, 54), QSize(kConflictW, 26)));

    m_led = new StatusLed(this);
    m_led->move(kControlX, 88 + 8);
    m_connectionText = new QLabel(this);
    m_connectionText->setObjectName(QStringLiteral("connectionText"));
    m_connectionText->setFont(Theme::font(Theme::Role::Body));
    m_connectionText->setProperty("state", QStringLiteral("error"));
    m_connectionText->setGeometry(QRect(QPoint(130, 88), QSize(kStatusElideW + 8, 20)));

    rule(QStringLiteral("MIDI INPUT"), 124);
    formLabel(QStringLiteral("PORT"), 144);
    m_inCombo = new QComboBox(this);
    m_inCombo->setObjectName(QStringLiteral("inputCombo"));
    itemDelegate(m_inCombo);
    m_inCombo->setGeometry(QRect(QPoint(kControlX, 144), QSize(260, 26)));
    m_useIn = useButton("useInputButton", 144);
    // The input problem line sits right of the USE button, where the output row has REFRESH.
    m_inputStatus = new QLabel(this);
    m_inputStatus->setObjectName(QStringLiteral("inputStatusText"));
    m_inputStatus->setFont(Theme::font(Theme::Role::Body));  // colour and size: QLabel#inputStatusText in app.qss
    m_inputStatus->setGeometry(QRect(QPoint(460, 144), QSize(100, 26)));
    m_follow = new QCheckBox(QStringLiteral("Follow the panel"), this);
    m_follow->setObjectName(QStringLiteral("followCheck"));
    m_follow->setGeometry(QRect(QPoint(kControlX, 178), QSize(Width - kControlX, 20)));
    m_resend = new QCheckBox(QStringLiteral("Resend the current patch on connect"), this);
    m_resend->setObjectName(QStringLiteral("resendCheck"));
    m_resend->setGeometry(QRect(QPoint(kControlX, 206), QSize(Width - kControlX, 20)));

    auto note = [&](int y, int height) {
        auto *n = new QLabel(noteFor(m_model), this);
        n->setProperty("role", QStringLiteral("note"));
        n->setFont(Theme::font(Theme::Role::Note));
        n->setWordWrap(true);
        n->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        n->setGeometry(QRect(QPoint(0, y), QSize(Width, height)));
    };
    const QString name = modelInfo(m_model).name;
    if (m_model == Model::JU06A) {
        rule(QStringLiteral("ENGINE"), 242);
        auto value = [&](int y) {
            auto *l = new QLabel(this);
            l->setProperty("role", QStringLiteral("engineValue"));
            l->setFont(Theme::font(Theme::Role::Body));
            l->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            l->setGeometry(QRect(QPoint(kControlX, y), QSize(140, 26)));  // "unknown" is wider than "106"
            return l;
        };
        auto wide = [&](int y, const QString &text, const char *role) {
            auto *l = new QLabel(text, this);
            l->setProperty("role", QString::fromLatin1(role));
            l->setFont(Theme::font(QLatin1String(role) == QLatin1String("dim") ? Theme::Role::Note : Theme::Role::Body));
            l->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            l->setGeometry(QRect(QPoint(160, y), QSize(Width - 160, 26)));
            return l;
        };
        formLabel(QStringLiteral("BELIEVED"), 262);
        m_believed = value(262);
        wide(262, QStringLiteral("%1 or the 60 / 106 buttons change it").arg(ctrlE()), "dim");
        formLabel(QStringLiteral("ON 60"), 288);
        m_on60 = value(288);
        m_on60Name = wide(288, QString(), "body");
        formLabel(QStringLiteral("ON 106"), 314);
        m_on106 = value(314);
        m_on106Name = wide(314, QString(), "body");
        rule(QStringLiteral("ON THE ") + name, 350);
        note(370, 45);
        setEngineInfo(EngineInfo());
    } else {
        rule(QStringLiteral("ON THE ") + name, 242);
        note(262, 30);
    }

    // Tab order follows the reading order.
    QWidget *order[] = {m_outCombo, m_useOut, m_refresh, m_channel, m_inCombo, m_useIn, m_follow, m_resend};
    for (size_t i = 0; i + 1 < sizeof(order) / sizeof(order[0]); ++i)
        setTabOrder(order[i], order[i + 1]);

    connect(m_refresh, &QPushButton::clicked, this, &DevicePage::refreshRequested);
    connect(m_outCombo, &QComboBox::currentIndexChanged, this, [this] {
        updateStatus();
        updateUseButtons();
        emit changed();
    });
    connect(m_inCombo, &QComboBox::currentIndexChanged, this, [this] {
        updateEnabled();
        updateInputStatus();
        updateUseButtons();
        emit changed();
    });
    connect(m_channel, &QSpinBox::valueChanged, this, [this] { emit changed(); });
    connect(m_follow, &QCheckBox::toggled, this, [this] { emit changed(); });
    connect(m_resend, &QCheckBox::toggled, this, [this] { emit changed(); });
    connect(m_useOut, &QPushButton::clicked, this, [this] {
        if (m_detectedOut.size() == 1)
            m_outCombo->setCurrentIndex(qMax(0, m_outCombo->findData(m_detectedOut.first())));
    });
    connect(m_useIn, &QPushButton::clicked, this, [this] {
        if (m_detectedIn.size() == 1)
            m_inCombo->setCurrentIndex(qMax(0, m_inCombo->findData(m_detectedIn.first())));
    });
    connect(&m_link, &MidiLink::inputStatusChanged, this, [this] { updateInputStatus(); emit changed(); });
    connect(&m_link, &MidiLink::statusChanged, this, [this] { updateStatus(); emit changed(); });
    connect(&m_link, &MidiLink::connectedChanged, this, [this] { updateStatus(); emit changed(); });
}

void DevicePage::setValues(const AppSettings::DeviceValues &v)
{
    m_initial = v;
    m_channel->setValue(v.channel);
    m_follow->setChecked(v.follow);
    m_resend->setChecked(v.resendOnConnect);
    // The combos are filled by setPorts(); until then they hold nothing and the wanted ports are
    // the initial ones.
}

AppSettings::DeviceValues DevicePage::values() const
{
    AppSettings::DeviceValues v;
    v.outputPort = m_outCombo->count() > 0 ? selectedKey(m_outCombo) : m_initial.outputPort;
    v.inputPort = m_inCombo->count() > 0 ? selectedKey(m_inCombo) : m_initial.inputPort;
    v.channel = m_channel->value();
    // Follow needs an input port: with None it is off whatever the (disabled) box shows.
    v.follow = m_follow->isChecked() && !v.inputPort.isEmpty();
    v.resendOnConnect = m_resend->isChecked();
    return v;
}

// Item text is the port key; two ports with the same key show their full names instead. A port
// this page's model owns reads "<text> . detected"; the item data stays the OS name.
void DevicePage::fillCombo(QComboBox *combo, const QStringList &names, const QList<Identify::Result> &ids,
                           const QString &wanted, bool withNone)
{
    QSignalBlocker block(combo);
    combo->clear();
    if (withNone)
        combo->addItem(QStringLiteral("None"), QString());
    QStringList keys;
    for (const QString &n : names)
        keys << MidiLink::portKey(n);
    QStringList &detected = withNone ? m_detectedIn : m_detectedOut;
    detected.clear();
    for (int i = 0; i < names.size(); ++i) {
        const bool dup = keys.count(keys.at(i)) > 1;
        const bool isMine = i < ids.size() && ids.at(i).model && *ids.at(i).model == m_model;
        QString text = dup ? names.at(i) : keys.at(i);
        if (isMine) {
            text += QStringLiteral(" · detected");
            detected << names.at(i);
        }
        combo->addItem(text, names.at(i));
        if (dup)
            combo->setItemData(combo->count() - 1, true, kIsFullRole);
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

void DevicePage::setPorts(const QStringList &outs, const QString &outError, const QList<Identify::Result> &outIds,
                          const QStringList &ins, const QString &inError, const QList<Identify::Result> &inIds)
{
    // Whatever is selected stays selected (or stays as a "missing" entry).
    const QString wantOut = m_outCombo->count() > 0 ? selectedKey(m_outCombo) : m_initial.outputPort;
    const QString wantIn = m_inCombo->count() > 0 ? selectedKey(m_inCombo) : m_initial.inputPort;
    m_listError = outError;
    m_outNames = outs;
    m_inListError = inError;
    m_inNames = ins;
    fillCombo(m_outCombo, outs, outIds, wantOut, false);
    fillCombo(m_inCombo, ins, inIds, wantIn, true);
    updateStatus();
    updateInputStatus();
    updateEnabled();
    updateUseButtons();
    emit changed();
}

// USE is enabled when exactly one port is detected for this model and the combo does not already
// hold it (DESIGN-2.md 7.3).
void DevicePage::updateUseButtons()
{
    auto apply = [](QPushButton *button, QComboBox *combo, const QStringList &detected) {
        const bool one = detected.size() == 1;
        button->setEnabled(one && combo->currentData().toString() != detected.first());
        button->setToolTip(one ? QStringLiteral("Use ") + MidiLink::portKey(detected.first()) : QString());
    };
    apply(m_useOut, m_outCombo, m_detectedOut);
    apply(m_useIn, m_inCombo, m_detectedIn);
}

void DevicePage::updateInputStatus()
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
    const QFontMetrics fm(Theme::font(Theme::Role::Body));
    m_inputStatus->setText(fm.elidedText(text, Qt::ElideRight, 100));
    m_inputStatus->setToolTip(text);
}

void DevicePage::updateEnabled()
{
    m_follow->setEnabled(!m_inCombo->currentData().toString().isEmpty());
}

QString DevicePage::connectionState() const
{
    return m_connectionText->property("state").toString();
}

void DevicePage::updateStatus()
{
    const QString selected = m_outCombo->currentData().toString();
    const QString key = MidiLink::portKey(selectedKey(m_outCombo));  // shown without ids
    QString text;
    bool ok = false;
    Kind kind = Kind::NotFound;
    if (!m_listError.isEmpty()) {
        text = QStringLiteral("Error: ") + m_listError;
        kind = Kind::Error;
    } else if (selected.isEmpty()) {
        text = QStringLiteral("Port not found");
        kind = Kind::NoPort;
    } else if (m_link.isConnected() && MidiLink::portKey(m_link.connectedPortName()) == key
               && !MidiLink::matchPort(m_outNames, selected).isEmpty()) {
        text = QStringLiteral("Connected to ") + key;
        ok = true;
        kind = Kind::Connected;
    } else if (!m_link.isConnected() && m_link.status().startsWith(QStringLiteral("Error"))
               && !MidiLink::matchPort({selected}, m_link.outputPortName()).isEmpty()) {
        // The wanted port (matched the way the link matches: "bout" finds "Boutique") failed.
        text = m_link.status();  // the port that is wanted failed to open: say why
        kind = Kind::Error;
    } else if (!MidiLink::matchPort(m_outNames, selected).isEmpty()) {
        // Listed: the port is there to be opened when OK applies (see the class comment).
        text = QStringLiteral("Connected to ") + key;
        ok = true;
        kind = Kind::Connected;
    } else {
        text = QStringLiteral("Port not found");
    }
    m_fullStatus = text;
    m_ok = ok;
    m_kind = kind;
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
}

void DevicePage::setConflictText(const QString &text)
{
    if (text == m_conflictFull)
        return;
    m_conflictFull = text;
    const QFontMetrics fm(Theme::font(Theme::Role::Note));
    m_conflictLabel->setText(fm.elidedText(text, Qt::ElideRight, kConflictW));
    m_conflictLabel->setToolTip(text);
}

void DevicePage::setEngineInfo(const EngineInfo &info)
{
    if (m_model != Model::JU06A)
        return;
    m_believed->setText(QString::number(info.engine));
    auto fill = [](QLabel *where, QLabel *name, const QString &on, const QString &patch) {
        const bool known = !on.isEmpty();
        where->setText(known ? on : QStringLiteral("unknown"));
        where->setProperty("role", known ? QStringLiteral("engineValue") : QStringLiteral("engineUnknown"));
        where->style()->unpolish(where);
        where->style()->polish(where);
        name->setText(known ? patch : QString());
    };
    fill(m_on60, m_on60Name, info.on60, info.name60);
    fill(m_on106, m_on106Name, info.on106, info.name106);
}

}  // namespace d05
