// The D-05 skin must render pixel for pixel as version 1.0.0 (DESIGN-2.md 1.5, 13.1; PLAN T8.1).
// tests/reference/ holds every version 1 --screenshot-state at 1x and 2x, rendered by the v1.0.0 tag
// with QT_QPA_PLATFORM=offscreen (QT_SCALE_FACTOR=2 for @2x). This test renders the same states from
// the built binary and compares the pixels exactly; a failure names the number of differing pixels
// and the first one.
//
// Text rasterisation is per platform (FreeType here, CoreText and DirectWrite elsewhere), so the
// comparison runs on Linux only. The states the later version 2 tasks change on purpose
// (DESIGN-2.md 13.1: the Settings dialog, the Shortcuts sheet's DEVICE section, the About box) stay in
// the set until T8.2 and T8.3 change them; whoever does that replaces those references in the same
// commit, with the designer's acceptance of the listed deviation.
#include <QImage>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

namespace {
// The Qt version the references in tests/reference/ were rendered with (see its README.md).
constexpr const char *kReferenceQt = "6.4.2";
}  // namespace

class TestReference : public QObject {
    Q_OBJECT
private slots:
    void d05StatesMatchVersion1_data()
    {
        QTest::addColumn<QString>("state");
        QTest::addColumn<int>("scale");
        const char *states[] = {"default", "search",   "favorites", "recents",        "disconnected",
                                "settings", "nomatch", "entry",     "rename",         "clearrecents",
                                "settings-error", "shortcuts", "about", "firstrun"};
        for (const char *s : states)
            for (const int scale : {1, 2})
                QTest::newRow(qPrintable(QStringLiteral("%1@%2x").arg(QLatin1String(s)).arg(scale)))
                    << QString::fromLatin1(s) << scale;
    }

    void d05StatesMatchVersion1()
    {
#ifndef Q_OS_LINUX
        QSKIP("the reference images were rendered on Linux; text rasterisation differs on other platforms");
#endif
        if (QString::fromLatin1(qVersion()) != QLatin1String(kReferenceQt))
            QSKIP(qPrintable(QStringLiteral("the references were rendered with Qt %1; this run uses Qt %2")
                                 .arg(QLatin1String(kReferenceQt), QString::fromLatin1(qVersion()))));
        QFETCH(QString, state);
        QFETCH(int, scale);
        QTemporaryDir out;
        QVERIFY(out.isValid());
        const QString png = out.filePath(QStringLiteral("shot.png"));
        QProcess p;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        env.insert(QStringLiteral("QT_SCALE_FACTOR"), QString::number(scale));
        p.setProcessEnvironment(env);
        p.setProcessChannelMode(QProcess::MergedChannels);
        p.start(QStringLiteral(D05_BINARY), {QStringLiteral("--screenshot"), png, QStringLiteral("--screenshot-state"), state});
        QVERIFY(p.waitForFinished(30000));
        if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0)
            qWarning().noquote() << "child output:" << QString::fromLocal8Bit(p.readAll());
        QCOMPARE(p.exitStatus(), QProcess::NormalExit);
        QCOMPARE(p.exitCode(), 0);

        QImage got;
        QVERIFY2(got.load(png), "the screenshot could not be read");
        QImage want;
        const QString refPath = QStringLiteral(D05_REFERENCE_DIR "/%1@%2x.png").arg(state).arg(scale);
        QVERIFY2(want.load(refPath), qPrintable(QStringLiteral("missing reference ") + refPath));
        QCOMPARE(got.size(), want.size());
        got = got.convertToFormat(QImage::Format_ARGB32);
        want = want.convertToFormat(QImage::Format_ARGB32);
        qint64 diffs = 0;
        QPoint first(-1, -1);
        for (int y = 0; y < want.height(); ++y) {
            const QRgb *a = reinterpret_cast<const QRgb *>(got.constScanLine(y));
            const QRgb *b = reinterpret_cast<const QRgb *>(want.constScanLine(y));
            for (int x = 0; x < want.width(); ++x) {
                if (a[x] != b[x]) {
                    if (diffs++ == 0)
                        first = QPoint(x, y);
                }
            }
        }
        if (diffs != 0) {
            const QString msg = QStringLiteral("%1 pixels differ from %2; first at (%3, %4): got #%5, reference #%6")
                                    .arg(diffs)
                                    .arg(refPath)
                                    .arg(first.x())
                                    .arg(first.y())
                                    .arg(got.pixel(first), 8, 16, QLatin1Char('0'))
                                    .arg(want.pixel(first), 8, 16, QLatin1Char('0'));
            QFAIL(qPrintable(msg));
        }
    }
};

QTEST_MAIN(TestReference)
#include "test_reference.moc"
