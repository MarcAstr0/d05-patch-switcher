#include "core/Identify.h"

#include <QFile>
#include <QRegularExpression>

#ifdef Q_OS_LINUX
#include <alsa/asoundlib.h>
#endif

namespace d05::Identify {

Result byPortName(const QString &portName)
{
    const auto has = [&](const char *s) {
        return portName.contains(QLatin1String(s), Qt::CaseInsensitive);
    };
    // Longest match first. "Boutique" also contains "Boutiq", so it is tested before it.
    if (has("JU-06A"))
        return {Model::JU06A, Confidence::Certain, false};
    if (has("JP-08"))
        return {Model::JP08, Confidence::Certain, false};
    if (has("D-05"))
        return {Model::D05, Confidence::Certain, false};
    if (has("Boutique"))
        return {Model::D05, Confidence::Likely, false};
    if (has("Boutiq"))
        return {Model::JP08, Confidence::Ambiguous, true};
    return {};
}

std::optional<Result> bySerial(const QString &usbId, const QString &serial)
{
    // The measured pairs (docs/IDENTIFY.md): the product id and the serial prefix must agree.
    struct Pair {
        const char *usbId;
        const char *prefix;
        Model model;
    };
    static const Pair pairs[] = {
        {"0582:01ff", "BQ_D", Model::D05},
        {"0582:01b5", "BQJP", Model::JP08},
        {"0582:023a", "BQJU", Model::JU06A},
    };
    const QString id = usbId.trimmed();
    const QString s = serial.trimmed();
    for (const Pair &p : pairs) {
        if (id.compare(QLatin1String(p.usbId), Qt::CaseInsensitive) == 0
            && s.startsWith(QLatin1String(p.prefix)))
            return Result{p.model, Confidence::Certain, false};
    }
    return std::nullopt;
}

namespace {
bool readFirstLine(const QString &path, QString *line)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    *line = QString::fromUtf8(f.readLine(256)).trimmed();
    return true;
}
}  // namespace

std::optional<Result> byCard(int card, const QString &procRoot, const QString &sysRoot)
{
    if (card < 0)
        return std::nullopt;
    QString usbId, serial;
    if (!readFirstLine(QStringLiteral("%1/asound/card%2/usbid").arg(procRoot).arg(card), &usbId)
        || !readFirstLine(QStringLiteral("%1/class/sound/card%2/device/../serial").arg(sysRoot).arg(card), &serial))
        return std::nullopt;
    return bySerial(usbId, serial);
}

std::optional<int> alsaClientOf(const QString &portName)
{
    static const QRegularExpression re(QStringLiteral("(\\d+):\\d+$"));
    const QRegularExpressionMatch m = re.match(portName);
    if (!m.hasMatch())
        return std::nullopt;
    bool ok = false;
    const int c = m.captured(1).toInt(&ok);
    return ok ? std::optional<int>(c) : std::nullopt;
}

#ifdef Q_OS_LINUX
namespace {
// alsa-lib prints failures (a sequencer that cannot be opened) through its default error handler
// on stderr; rule 2 fails silently, so a no-op handler is installed for the scope.
void quietAlsaHandler(const char *, int, const char *, int, const char *, ...) {}
class QuietAlsa {
public:
    QuietAlsa() : m_previous(snd_lib_error) { snd_lib_error_set_handler(quietAlsaHandler); }
    ~QuietAlsa() { snd_lib_error_set_handler(m_previous); }
    QuietAlsa(const QuietAlsa &) = delete;
    QuietAlsa &operator=(const QuietAlsa &) = delete;

private:
    snd_lib_error_handler_t m_previous;
};

// The sound card behind an ALSA sequencer client, or -1. Opens one sequencer handle for the lot
// and no port; any failure is silent.
class SeqCards {
public:
    SeqCards()
    {
        QuietAlsa quiet;
        if (snd_seq_open(&m_seq, "default", SND_SEQ_OPEN_INPUT, 0) < 0)
            m_seq = nullptr;
    }
    ~SeqCards()
    {
        if (m_seq)
            snd_seq_close(m_seq);
    }
    SeqCards(const SeqCards &) = delete;
    SeqCards &operator=(const SeqCards &) = delete;

    int cardOf(int client) const
    {
        if (!m_seq)
            return -1;
        QuietAlsa quiet;
        snd_seq_client_info_t *info = nullptr;
        if (snd_seq_client_info_malloc(&info) < 0)
            return -1;
        int card = -1;
        if (snd_seq_get_any_client_info(m_seq, client, info) >= 0)
            card = snd_seq_client_info_get_card(info);
        snd_seq_client_info_free(info);
        return card;
    }

private:
    snd_seq_t *m_seq = nullptr;
};
}  // namespace
#endif

QList<Result> identifyPorts(const QStringList &portNames, bool useSystemLookup)
{
    QList<Result> out;
    out.reserve(portNames.size());
#ifdef Q_OS_LINUX
    std::optional<SeqCards> cards;
#else
    Q_UNUSED(useSystemLookup);
#endif
    for (const QString &name : portNames) {
        Result r = byPortName(name);
#ifdef Q_OS_LINUX
        if (useSystemLookup) {
            if (const auto client = alsaClientOf(name)) {
                if (!cards)
                    cards.emplace();
                if (const auto refined = byCard(cards->cardOf(*client), QStringLiteral("/proc"), QStringLiteral("/sys")))
                    r = *refined;
            }
        }
#endif
        out.append(r);
    }
    return out;
}

QString confidenceText(Confidence c)
{
    switch (c) {
    case Confidence::Certain: return QStringLiteral("certain");
    case Confidence::Likely: return QStringLiteral("likely");
    case Confidence::Ambiguous: return QStringLiteral("ambiguous");
    case Confidence::None: break;
    }
    return QStringLiteral("none");
}

}  // namespace d05::Identify
