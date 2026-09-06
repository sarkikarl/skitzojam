// audioengine.h
//
// WASAPI loopback-capture + real-time "8D audio" effect engine, wrapped
// as a QObject so the UI can drive it with signals/slots instead of
// hand-rolled cross-thread messaging.
//
// Threading model: start()/stop() spawn/join a plain std::thread that
// does the actual WASAPI work (same approach as the original Win32
// version). The worker thread emits signals (statusChanged, etc.)
// directly — Qt automatically marshals those to the GUI thread via a
// queued connection as long as the receiving object lives there, which
// is the whole reason this is dramatically simpler and safer than the
// manual PostMessage hack the raw Win32 version needed to avoid a
// deadlock.
//
// Effect parameters (speed/depth/floor/randomization) are plain
// std::atomic<double> members — settable from the GUI thread at any
// time, read by the audio thread once per buffer, exactly like before.

#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include <thread>
#include <random>
#include <vector>

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

struct AudioDeviceInfo {
    QString name;
    QString id; // stable Windows endpoint ID
};

class AudioEngine : public QObject {
    Q_OBJECT

public:
    explicit AudioEngine(QObject *parent = nullptr);
    ~AudioEngine() override;

    // Enumerates devices capable of loopback capture (i.e. render/output
    // endpoints — loopback always captures FROM a render device). Safe
    // to call from the GUI thread; does its own COM init/uninit locally.
    static std::vector<AudioDeviceInfo> listDevices();

    void setLfoHz(double hz) { lfoHz_ = hz; }
    void setDepth(double depth) { depth_ = depth; }
    void setMinGain(double g) { minGain_ = g; }
    void setRandomAmount(double r) { randomAmount_ = r; }

    double lfoHz() const { return lfoHz_.load(); }
    double depth() const { return depth_.load(); }
    double minGain() const { return minGain_.load(); }
    double randomAmount() const { return randomAmount_.load(); }

public slots:
    // Stops whatever's running, then starts fresh with the given device
    // IDs — unless they're the same device, in which case it refuses
    // and emits a status explaining why (see the .cpp for the reasoning
    // on why same-device is blocked).
    void restart(const QString &captureId, const QString &renderId);
    void stop();

signals:
    void statusChanged(const QString &status);

private:
    void audioThreadProc(QString captureId, QString renderId);

    std::atomic<double> lfoHz_{0.35};
    std::atomic<double> depth_{0.7};
    std::atomic<double> minGain_{0.45};
    std::atomic<double> randomAmount_{0.0};

    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
    std::thread thread_;
};
