#include "ui/SettingsDialog.h"

#include "core/PatchCatalog.h"
#include "midi/DeviceRouter.h"
#include "midi/MidiLink.h"
#include "ui/DeviceListWidget.h"
#include "ui/SectionRule.h"
#include "ui/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFontMetrics>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

namespace d05 {

namespace {
constexpr int kMargin = 20;
constexpr int kContentW = 720;

QString portLabel(const QString &key)
{
    return key.isEmpty() ? QStringLiteral("no port") : MidiLink::portKey(key);
}
}  // namespace

SettingsDialog::SettingsDialog(DeviceRouter &router, const State &initial, Model shown, QWidget *parent,
                               bool systemLookup)
    : QDialog(parent)
    , m_router(router)
    , m_systemLookup(systemLookup)
{
    setObjectName(QStringLiteral("settingsDialog"));
    setWindowTitle(QStringLiteral("Settings"));
    setModal(true);
    setFixedSize(ClientWidth, ClientHeight);
    buildUi(shown, initial);
    refreshPorts();
    refreshEngineInfo();
    updateAll();
    currentPage()->outputCombo()->setFocus();  // DESIGN 10.1: initial focus
}

void SettingsDialog::buildUi(Model shown, const State &initial)
{
    m_shown = shown;
    auto at = [](int x, int y) { return QPoint(kMargin + x, kMargin + y); };

    m_list = new DeviceListWidget(this);
    m_list->move(at(0, 0));
    for (Model m : allModels()) {
        m_enabled[int(m)] = initial.devices[int(m)].enabled;
        auto *page = new DevicePage(m, *m_router.link(m), this);
        page->move(at(160, 0));
        page->setValues(initial.devices[int(m)]);
        m_pages[int(m)] = page;
        page->setVisible(m == shown);
        connect(page, &DevicePage::changed, this, [this] { updateAll(); });
        connect(page, &DevicePage::refreshRequested, this, [this] { refreshPorts(); });
    }
    m_list->setCurrent(shown);
    connect(m_list, &DeviceListWidget::currentChanged, this, [this](Model m) { setShown(m); });
    connect(m_list, &DeviceListWidget::enabledToggled, this, [this](Model m, bool on) {
        m_enabled[int(m)] = on;
        updateAll();
    });

    auto *rule = new SectionRule(QStringLiteral("ALL DEVICES"), false, this);
    rule->setGeometry(QRect(at(0, 439), QSize(kContentW, SectionRule::Height)));
    m_autoSend = new QCheckBox(QStringLiteral("Auto-send on highlight"), this);
    m_autoSend->setObjectName(QStringLiteral("autoSendCheck"));
    m_autoSend->setGeometry(QRect(at(272, 459), QSize(kContentW - 272, 20)));
    m_autoSend->setChecked(initial.autoSend);
    m_followSwitches = new QCheckBox(QStringLiteral("Follow switches device"), this);
    m_followSwitches->setObjectName(QStringLiteral("followSwitchesCheck"));
    m_followSwitches->setGeometry(QRect(at(272, 487), QSize(kContentW - 272, 20)));
    m_followSwitches->setChecked(initial.followSwitches);
    auto *hint = new QLabel(QStringLiteral("An incoming patch change from another device switches the window to it."), this);
    hint->setProperty("role", QStringLiteral("dim"));
    hint->setFont(Theme::font(Theme::Role::Note));
    hint->setGeometry(QRect(at(298, 507), QSize(420, 16)));

    constexpr int buttonY = 542;
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

    // Tab order: the device list, the pages (top to bottom), the global checkboxes, TEST, CANCEL, OK.
    QList<QWidget *> order{m_list};
    for (Model m : allModels()) {
        DevicePage *p = m_pages[int(m)];
        order << p->outputCombo() << p->useOutputButton() << p->refreshButton() << p->channelSpin() << p->inputCombo()
              << p->useInputButton() << p->followCheck() << p->resendCheck();
    }
    order << m_autoSend << m_followSwitches << m_test << m_cancel << m_ok;
    for (int i = 0; i + 1 < order.size(); ++i)
        setTabOrder(order[i], order[i + 1]);

    connect(m_ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_test, &QPushButton::clicked, this, [this] {
        m_testPressed[int(m_shown)] = true;
        emit testRequested(m_shown, deviceValues(m_shown));
    });
}

void SettingsDialog::showTestRefused(Model m, Model other)
{
    const AppSettings::DeviceValues v = deviceValues(m);
    m_pages[int(m)]->setConflictText(QStringLiteral("Channel %1 on %2 is used by the %3.")
                                         .arg(v.channel).arg(portLabel(v.outputPort), modelInfo(other).name));
}

void SettingsDialog::setShown(Model m)
{
    if (m == m_shown && m_pages[int(m)]->isVisibleTo(this))
        return;
    m_shown = m;
    for (Model o : allModels())
        m_pages[int(o)]->setVisible(o == m);
    m_list->setCurrent(m);
    updateAll();
}

// Reads the port lists once and hands them, with what Identify makes of each name, to every page.
void SettingsDialog::refreshPorts()
{
    QString outErr, inErr;
    const MidiLink *lister = m_router.link(Model::D05);
    const QStringList outs = lister->outputPortNames(&outErr);
    const QStringList ins = lister->inputPortNames(&inErr);
    const QList<Identify::Result> outIds = Identify::identifyPorts(outs, m_systemLookup);
    const QList<Identify::Result> inIds = Identify::identifyPorts(ins, m_systemLookup);
    for (Model m : allModels())
        m_pages[int(m)]->setPorts(outs, outErr, outIds, ins, inErr, inIds);
}

// The JU-06A page's read-only information: the believed engine and the remembered program of each
// engine with its factory name.
void SettingsDialog::refreshEngineInfo()
{
    const EngineTracker *t = m_router.ju06aTracker();
    if (!t)
        return;
    static const PatchCatalog catalog(Model::JU06A);
    DevicePage::EngineInfo info;
    info.engine = t->engine();
    const auto describe = [&](int engine, QString *on, QString *name) {
        const int program = t->remembered(engine);
        if (program < 0)
            return;
        const auto a = PatchAddress::fromBankNumber(Model::JU06A, program / 8 + 1, program % 8 + 1, engine);
        if (!a)
            return;
        *on = QStringLiteral("%1-%2").arg(a->bank()).arg(a->number());
        *name = catalog.name(*a);
    };
    describe(60, &info.on60, &info.name60);
    describe(106, &info.on106, &info.name106);
    m_pages[int(Model::JU06A)]->setEngineInfo(info);
}

AppSettings::DeviceValues SettingsDialog::deviceValues(Model m) const
{
    AppSettings::DeviceValues v = m_pages[int(m)]->values();
    v.enabled = m_enabled[int(m)];
    return v;
}

SettingsDialog::State SettingsDialog::state() const
{
    State s;
    for (Model m : allModels())
        s.devices[int(m)] = deviceValues(m);
    s.autoSend = m_autoSend->isChecked();
    s.followSwitches = m_followSwitches->isChecked();
    return s;
}

AppSettings::Values SettingsDialog::values() const
{
    const AppSettings::DeviceValues d = deviceValues(Model::D05);
    AppSettings::Values v;
    v.outputPort = d.outputPort;
    v.inputPort = d.inputPort;
    v.channel = d.channel;
    v.autoSend = m_autoSend->isChecked();
    v.follow = d.follow;
    v.resendOnConnect = d.resendOnConnect;
    return v;
}

// "Channel 4 on Hapax MIDI 1 is used by the D-05." (DESIGN-2.md 7.3); for inputs "Channel 4 on
// JU-06A (input) is used by the D-05." Empty when device `m` collides with no earlier device.
QString SettingsDialog::conflictTextFor(Model m, const DeviceRouter &) const
{
    DeviceRouter::Configs cfgs;
    for (Model o : allModels()) {
        const AppSettings::DeviceValues v = deviceValues(o);
        DeviceRouter::DeviceConfig c;
        c.enabled = v.enabled;
        c.outputPort = v.outputPort;
        c.inputPort = v.inputPort;
        c.channel = v.channel;
        c.follow = v.follow;
        cfgs[int(o)] = c;
    }
    const auto conflict = DeviceRouter::findConflict(cfgs, m, true);
    if (!conflict)
        return QString();
    const DeviceRouter::DeviceConfig &mine = cfgs[int(m)];
    const QString port = portLabel(conflict->input ? mine.inputPort : mine.outputPort);
    return QStringLiteral("Channel %1 on %2%3 is used by the %4.")
        .arg(mine.channel)
        .arg(port, conflict->input ? QStringLiteral(" (input)") : QString(), modelInfo(conflict->other).name);
}

// Everything that depends on more than one control: the list rows (enabled, connection), the
// conflict texts, OK (disabled while any conflict exists) and TEST (the shown device must be
// enabled, its output listed, and free of conflict).
void SettingsDialog::updateAll()
{
    bool conflict = false;
    for (Model m : allModels()) {
        const QString text = conflictTextFor(m, m_router);
        m_pages[int(m)]->setConflictText(text);
        conflict = conflict || !text.isEmpty();

        DeviceListWidget::Row row;
        row.enabled = m_enabled[int(m)];
        const DevicePage *p = m_pages[int(m)];
        if (!row.enabled) {
            row.text = QStringLiteral("Off");
            row.tone = DeviceListWidget::Tone::Dim;
        } else {
            switch (p->kind()) {
            case DevicePage::Kind::Connected:
                row.text = QStringLiteral("Connected");
                row.tone = DeviceListWidget::Tone::Secondary;
                row.lit = true;
                break;
            case DevicePage::Kind::NotFound:
                row.text = QStringLiteral("Port not found");
                row.tone = DeviceListWidget::Tone::Red;
                break;
            case DevicePage::Kind::Error:
                row.text = QStringLiteral("Error");
                row.tone = DeviceListWidget::Tone::Red;
                row.tooltip = p->connectionText();
                break;
            case DevicePage::Kind::NoPort:
                row.text = QStringLiteral("No output port");
                row.tone = DeviceListWidget::Tone::Dim;
                break;
            }
        }
        m_list->setRow(m, row);
    }
    m_conflict = conflict;
    m_ok->setEnabled(!conflict);
    const DevicePage *shown = currentPage();
    m_test->setEnabled(m_enabled[int(m_shown)] && shown->connectionOk() && shown->conflictText().isEmpty());
}

}  // namespace d05
