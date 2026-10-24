// Construction failures of RtMidi must surface as errors, not as an empty port list.
#include "app/Cli.h"
#include "midi/RtMidiBackend.h"

#include <QTest>
#include <QTextStream>

using namespace d05;

class TestRtMidiInit : public QObject {
    Q_OBJECT
private slots:
    void bothFailReportErrors()
    {
        RtMidiBackend b(RtMidiBackend::SimulateFailure::Both);
        QString e;
        QVERIFY(b.outputPortNames(&e).isEmpty());
        QVERIFY(!e.isEmpty());
        e.clear();
        QVERIFY(b.inputPortNames(&e).isEmpty());
        QVERIFY(!e.isEmpty());
        QString oe;
        QVERIFY(!b.openOutput(QStringLiteral("x"), &oe));
        QVERIFY(!oe.isEmpty());
    }

    void cliExitsThree()
    {
        RtMidiBackend b(RtMidiBackend::SimulateFailure::Both);
        QString o, er;
        QTextStream out(&o), err(&er);
        QCOMPARE(Cli::runListPorts(b, out, err), Exit::MidiError);
        CliOptions opts;
        opts.hasSend = true;
        opts.sendAddress = QStringLiteral("P1-11");
        opts.port = QStringLiteral("Midi Through");
        QCOMPARE(Cli::runSend(opts, b, out, err), Exit::MidiError);
    }

    void inputFailureKeepsOutput()
    {
        RtMidiBackend b(RtMidiBackend::SimulateFailure::Input);
        QString e;
        b.outputPortNames(&e);
        if (!e.isEmpty())
            QSKIP("no working MIDI system on this machine");
        QVERIFY(e.isEmpty());
        e.clear();
        QVERIFY(b.inputPortNames(&e).isEmpty());
        QVERIFY(e.contains(QStringLiteral("simulated input")));
    }

    void outputFailureKeepsInput()
    {
        RtMidiBackend b(RtMidiBackend::SimulateFailure::Output);
        QString e;
        b.inputPortNames(&e);
        if (!e.isEmpty())
            QSKIP("no working MIDI system on this machine");
        e.clear();
        QVERIFY(b.outputPortNames(&e).isEmpty());
        QVERIFY(e.contains(QStringLiteral("simulated output")));
    }
};

QTEST_GUILESS_MAIN(TestRtMidiInit)
#include "test_rtmidi_init.moc"
