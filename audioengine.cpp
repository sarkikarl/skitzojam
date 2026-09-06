// audioengine.cpp

#include "audioengine.h"

#include <functiondiscoverykeys_devpkey.h>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------------
// One-pole lowpass filter
// ---------------------------------------------------------------------
namespace {

struct OnePoleLPF {
    float state = 0.0f;
    float process(float x, float alpha) {
        state += alpha * (x - state);
        return state;
    }
};

// ---------------------------------------------------------------------
// The actual "8D" DSP: equal-power autopan + distance filtering +
// organic randomization. Reads current parameter values from the
// engine's atomics every call, so slider changes apply live.
// ---------------------------------------------------------------------
class EightDEffect {
public:
    EightDEffect(double sampleRate, const AudioEngine *engine)
        : sr_(sampleRate), engine_(engine), rng_(std::random_device{}()) {
        pickNewRandomTargets();
    }

    void process(float *interleavedStereo, UINT32 numFrames) {
        const double twoPi = 6.283185307179586;
        double lfoHz = engine_->lfoHz();
        double depth = engine_->depth();
        double minGain = engine_->minGain();
        double lpfFloor = minGain;
        double randomAmount = engine_->randomAmount();

        for (UINT32 n = 0; n < numFrames; ++n) {
            samplesUntilRetarget_ -= 1.0;
            if (samplesUntilRetarget_ <= 0.0) {
                pickNewRandomTargets();
                samplesUntilRetarget_ = sr_ * (0.6 + 1.2 * dist01_(rng_));
            }
            double smoothing = 0.0005;
            speedMultSmoothed_ += smoothing * (speedMultTarget_ - speedMultSmoothed_);
            depthMultSmoothed_ += smoothing * (depthMultTarget_ - depthMultSmoothed_);

            double effectiveLfoHz = lfoHz * (1.0 + randomAmount * speedMultSmoothed_);
            double effectiveDepth = depth * (1.0 + randomAmount * depthMultSmoothed_);
            if (effectiveDepth < 0.0) effectiveDepth = 0.0;
            if (effectiveDepth > 1.0) effectiveDepth = 1.0;

            double lfo = std::sin(twoPi * effectiveLfoHz * phase_);
            phase_ += 1.0 / sr_;
            if (phase_ > 1e6) phase_ = std::fmod(phase_, 3600.0);

            double panPos = lfo * effectiveDepth;
            double angle = (panPos + 1.0) * (M_PI / 4.0);
            double rawGainL = std::cos(angle);
            double rawGainR = std::sin(angle);

            float gainL = static_cast<float>(minGain + (1.0 - minGain) * rawGainL);
            float gainR = static_cast<float>(minGain + (1.0 - minGain) * rawGainR);

            float inL = interleavedStereo[2 * n];
            float inR = interleavedStereo[2 * n + 1];

            float alphaL = static_cast<float>(lpfFloor) + (1.0f - static_cast<float>(lpfFloor)) * gainL;
            float alphaR = static_cast<float>(lpfFloor) + (1.0f - static_cast<float>(lpfFloor)) * gainR;

            interleavedStereo[2 * n]     = lpfL_.process(inL * gainL, alphaL);
            interleavedStereo[2 * n + 1] = lpfR_.process(inR * gainR, alphaR);
        }
    }

private:
    void pickNewRandomTargets() {
        speedMultTarget_ = (dist01_(rng_) * 2.0 - 1.0) * 0.30;
        depthMultTarget_ = (dist01_(rng_) * 2.0 - 1.0) * 0.20;
    }

    double sr_;
    const AudioEngine *engine_;
    double phase_ = 0.0;
    OnePoleLPF lpfL_, lpfR_;

    std::mt19937 rng_;
    std::uniform_real_distribution<double> dist01_{0.0, 1.0};
    double speedMultTarget_ = 0.0, speedMultSmoothed_ = 0.0;
    double depthMultTarget_ = 0.0, depthMultSmoothed_ = 0.0;
    double samplesUntilRetarget_ = 0.0;
};

} // namespace

AudioEngine::AudioEngine(QObject *parent) : QObject(parent) {}

AudioEngine::~AudioEngine() {
    stop();
}

std::vector<AudioDeviceInfo> AudioEngine::listDevices() {
    std::vector<AudioDeviceInfo> result;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool weInitialized = SUCCEEDED(hr);
    // RPC_E_CHANGED_MODE means COM was already initialized differently on
    // this thread — that's fine, we just don't uninit it ourselves later.

    IMMDeviceEnumerator *enumr = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                           __uuidof(IMMDeviceEnumerator), (void **)&enumr);
    if (SUCCEEDED(hr)) {
        IMMDeviceCollection *collection = nullptr;
        // Loopback always captures FROM a render (output) device, so
        // that's the flow we enumerate for both dropdowns.
        if (SUCCEEDED(enumr->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection))) {
            UINT count = 0;
            collection->GetCount(&count);
            for (UINT i = 0; i < count; ++i) {
                IMMDevice *dev = nullptr;
                collection->Item(i, &dev);

                IPropertyStore *props = nullptr;
                dev->OpenPropertyStore(STGM_READ, &props);
                PROPVARIANT nameVar;
                PropVariantInit(&nameVar);
                props->GetValue(PKEY_Device_FriendlyName, &nameVar);

                AudioDeviceInfo info;
                info.name = nameVar.pwszVal
                                ? QString::fromWCharArray(nameVar.pwszVal)
                                : QStringLiteral("(unknown)");

                LPWSTR idStr = nullptr;
                if (SUCCEEDED(dev->GetId(&idStr)) && idStr) {
                    info.id = QString::fromWCharArray(idStr);
                    CoTaskMemFree(idStr);
                }

                result.push_back(info);

                PropVariantClear(&nameVar);
                props->Release();
                dev->Release();
            }
            collection->Release();
        }
        enumr->Release();
    }

    if (weInitialized) CoUninitialize();
    return result;
}

void AudioEngine::restart(const QString &captureId, const QString &renderId) {
    stop();

    if (captureId.isEmpty() || renderId.isEmpty()) {
        emit statusChanged("Pick an input and output device");
        return;
    }
    if (captureId == renderId) {
        emit statusChanged("Input and output must be different devices");
        return;
    }

    stopRequested_ = false;
    running_ = true;
    thread_ = std::thread(&AudioEngine::audioThreadProc, this, captureId, renderId);
}

void AudioEngine::stop() {
    if (running_.load()) {
        stopRequested_ = true;
        if (thread_.joinable()) thread_.join();
    }
}

void AudioEngine::audioThreadProc(QString captureId, QString renderId) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) { emit statusChanged("Failed to init COM on audio thread"); running_ = false; return; }

    IMMDeviceEnumerator *enumr = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                           __uuidof(IMMDeviceEnumerator), (void **)&enumr);
    if (FAILED(hr)) { emit statusChanged("Failed to create device enumerator"); CoUninitialize(); running_ = false; return; }

    std::wstring captureIdW = captureId.toStdWString();
    std::wstring renderIdW = renderId.toStdWString();

    IMMDevice *captureDevice = nullptr;
    IMMDevice *renderDevice = nullptr;
    hr = enumr->GetDevice(captureIdW.c_str(), &captureDevice);
    if (FAILED(hr)) { emit statusChanged("Capture device unavailable"); enumr->Release(); CoUninitialize(); running_ = false; return; }
    hr = enumr->GetDevice(renderIdW.c_str(), &renderDevice);
    if (FAILED(hr)) {
        emit statusChanged("Render device unavailable");
        captureDevice->Release(); enumr->Release(); CoUninitialize(); running_ = false; return;
    }

    IAudioClient *captureClient = nullptr;
    hr = captureDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void **)&captureClient);
    if (FAILED(hr)) {
        emit statusChanged("Failed to activate capture device");
        captureDevice->Release(); renderDevice->Release(); enumr->Release(); CoUninitialize(); running_ = false; return;
    }

    WAVEFORMATEX *captureFmt = nullptr;
    captureClient->GetMixFormat(&captureFmt);

    hr = captureClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
                                    10 * 1000000, 0, captureFmt, nullptr);
    if (FAILED(hr)) {
        emit statusChanged("Failed to init loopback capture");
        CoTaskMemFree(captureFmt); captureClient->Release();
        captureDevice->Release(); renderDevice->Release(); enumr->Release(); CoUninitialize(); running_ = false; return;
    }

    IAudioCaptureClient *captureService = nullptr;
    captureClient->GetService(__uuidof(IAudioCaptureClient), (void **)&captureService);

    IAudioClient *renderClient = nullptr;
    hr = renderDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void **)&renderClient);
    if (FAILED(hr)) {
        emit statusChanged("Failed to activate render device");
        CoTaskMemFree(captureFmt); captureService->Release(); captureClient->Release();
        captureDevice->Release(); renderDevice->Release(); enumr->Release(); CoUninitialize(); running_ = false; return;
    }

    WAVEFORMATEX *renderFmt = nullptr;
    renderClient->GetMixFormat(&renderFmt);

    hr = renderClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10 * 1000000, 0, renderFmt, nullptr);
    if (FAILED(hr)) {
        emit statusChanged("Failed to init render client");
        CoTaskMemFree(captureFmt); CoTaskMemFree(renderFmt);
        captureService->Release(); captureClient->Release(); renderClient->Release();
        captureDevice->Release(); renderDevice->Release(); enumr->Release(); CoUninitialize(); running_ = false; return;
    }

    IAudioRenderClient *renderService = nullptr;
    renderClient->GetService(__uuidof(IAudioRenderClient), (void **)&renderService);

    if (captureFmt->nSamplesPerSec != renderFmt->nSamplesPerSec) {
        emit statusChanged(QString("WARNING sample rates differ (%1 vs %2 Hz)")
                                .arg(captureFmt->nSamplesPerSec).arg(renderFmt->nSamplesPerSec));
    } else {
        emit statusChanged("Running");
    }

    EightDEffect effect(captureFmt->nSamplesPerSec, this);

    captureClient->Start();
    renderClient->Start();

    std::vector<float> stereoScratch;

    while (!stopRequested_.load()) {
        UINT32 packetLength = 0;
        if (FAILED(captureService->GetNextPacketSize(&packetLength))) break;

        while (packetLength != 0 && !stopRequested_.load()) {
            BYTE *capturedData = nullptr;
            UINT32 numFrames = 0;
            DWORD flags = 0;

            if (FAILED(captureService->GetBuffer(&capturedData, &numFrames, &flags, nullptr, nullptr)))
                break;

            float *samples = reinterpret_cast<float *>(capturedData);
            UINT32 channels = captureFmt->nChannels;

            stereoScratch.resize((size_t)numFrames * 2);
            if (channels == 2) {
                memcpy(stereoScratch.data(), samples, sizeof(float) * numFrames * 2);
            } else if (channels == 1) {
                for (UINT32 i = 0; i < numFrames; ++i) {
                    stereoScratch[2 * i] = samples[i];
                    stereoScratch[2 * i + 1] = samples[i];
                }
            } else {
                for (UINT32 i = 0; i < numFrames; ++i) {
                    float l = samples[i * channels + 0];
                    float r = (channels > 1) ? samples[i * channels + 1] : l;
                    stereoScratch[2 * i] = l;
                    stereoScratch[2 * i + 1] = r;
                }
            }

            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                effect.process(stereoScratch.data(), numFrames);
            }

            BYTE *renderData = nullptr;
            if (SUCCEEDED(renderService->GetBuffer(numFrames, &renderData))) {
                float *outSamples = reinterpret_cast<float *>(renderData);
                UINT32 renderChannels = renderFmt->nChannels;
                if (renderChannels == 2) {
                    memcpy(outSamples, stereoScratch.data(), sizeof(float) * numFrames * 2);
                } else {
                    for (UINT32 i = 0; i < numFrames; ++i)
                        for (UINT32 c = 0; c < renderChannels; ++c)
                            outSamples[i * renderChannels + c] =
                                (c % 2 == 0) ? stereoScratch[2 * i] : stereoScratch[2 * i + 1];
                }
                renderService->ReleaseBuffer(numFrames, 0);
            }

            captureService->ReleaseBuffer(numFrames);
            if (FAILED(captureService->GetNextPacketSize(&packetLength))) break;
        }

        Sleep(5);
    }

    captureClient->Stop();
    renderClient->Stop();

    CoTaskMemFree(captureFmt);
    CoTaskMemFree(renderFmt);
    captureService->Release();
    renderService->Release();
    captureClient->Release();
    renderClient->Release();
    captureDevice->Release();
    renderDevice->Release();
    enumr->Release();

    CoUninitialize();
    emit statusChanged("Stopped");
    running_ = false;
}
