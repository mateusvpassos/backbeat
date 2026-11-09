#include "capture/ProcessLoopbackCapture.h"

#ifndef NOMINMAX
 #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <tlhelp32.h>

#include <map>
#include <vector>

#pragma comment (lib, "mmdevapi.lib")
#pragma comment (lib, "ole32.lib")

namespace bb
{

namespace
{
    // Recebe o resultado do ActivateAudioInterfaceAsync. Precisa ser "agile" (pode ser chamado de qualquer thread).
    class ActivationHandler final : public IActivateAudioInterfaceCompletionHandler, public IAgileObject
    {
    public:
        ActivationHandler() { done = CreateEventW (nullptr, TRUE, FALSE, nullptr); }
        ~ActivationHandler() { CloseHandle (done); }

        ULONG STDMETHODCALLTYPE AddRef() override  { return (ULONG) ++refs; }
        ULONG STDMETHODCALLTYPE Release() override
        {
            const auto r = --refs;
            if (r == 0)
                delete this;
            return (ULONG) r;
        }

        HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void** out) override
        {
            if (riid == __uuidof (IUnknown) || riid == __uuidof (IActivateAudioInterfaceCompletionHandler))
                *out = static_cast<IActivateAudioInterfaceCompletionHandler*> (this);
            else if (riid == __uuidof (IAgileObject))
                *out = static_cast<IAgileObject*> (this);
            else
            {
                *out = nullptr;
                return E_NOINTERFACE;
            }
            AddRef();
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE ActivateCompleted (IActivateAudioInterfaceAsyncOperation* op) override
        {
            IUnknown* unknown = nullptr;
            HRESULT activateResult = E_FAIL;
            const HRESULT hr = op->GetActivateResult (&activateResult, &unknown);
            result = FAILED (hr) ? hr : activateResult;

            if (SUCCEEDED (result) && unknown != nullptr)
                unknown->QueryInterface (__uuidof (IAudioClient), reinterpret_cast<void**> (&client));
            if (unknown != nullptr)
                unknown->Release();

            SetEvent (done);
            return S_OK;
        }

        HANDLE done = nullptr;
        HRESULT result = E_PENDING;
        IAudioClient* client = nullptr; // referência passa para quem pegar

    private:
        std::atomic<long> refs { 1 };
    };

    template <typename T>
    struct ComPtr
    {
        ~ComPtr() { if (p != nullptr) p->Release(); }
        T* p = nullptr;
        T** operator&() { return &p; }
        T* operator->() const { return p; }
        explicit operator bool() const { return p != nullptr; }
    };

    juce::String hresultText (HRESULT hr)
    {
        switch (hr)
        {
            case E_NOTIMPL:                     return L"recurso não suportado (precisa do Windows 10 2004 ou mais novo)";
            case AUDCLNT_E_UNSUPPORTED_FORMAT:  return L"formato de áudio recusado";
            case AUDCLNT_E_DEVICE_INVALIDATED:  return L"dispositivo de áudio mudou";
            case E_ACCESSDENIED:                return "acesso negado";
            default:                            return "erro 0x" + juce::String::toHexString ((juce::int64) (std::uint32_t) hr);
        }
    }

    WAVEFORMATEX makeFormat (double sampleRate, bool asFloat)
    {
        WAVEFORMATEX f {};
        f.wFormatTag = asFloat ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM;
        f.nChannels = 2;
        f.nSamplesPerSec = (DWORD) juce::roundToInt (sampleRate);
        f.wBitsPerSample = asFloat ? 32 : 16;
        f.nBlockAlign = (WORD) (f.nChannels * f.wBitsPerSample / 8);
        f.nAvgBytesPerSec = f.nSamplesPerSec * f.nBlockAlign;
        f.cbSize = 0;
        return f;
    }
}

// ---------------------------------------------------------------------------
// Processos
// ---------------------------------------------------------------------------

std::uint32_t ProcessLoopbackCapture::findMainProcess (const juce::String& exeName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    std::map<DWORD, DWORD> parentOf;      // pid → pai, só dos processos com o nome procurado
    PROCESSENTRY32W entry {};
    entry.dwSize = sizeof (entry);

    if (Process32FirstW (snapshot, &entry))
    {
        do
        {
            if (exeName.equalsIgnoreCase (juce::String (entry.szExeFile)))
                parentOf[entry.th32ProcessID] = entry.th32ParentProcessID;
        }
        while (Process32NextW (snapshot, &entry));
    }
    CloseHandle (snapshot);

    // O processo principal é o que não tem outro processo de mesmo nome como pai.
    // Capturando a árvore dele, entra o processo de áudio do Chrome (que é filho).
    for (const auto& [pid, parent] : parentOf)
        if (parentOf.count (parent) == 0)
            return (std::uint32_t) pid;

    return 0;
}

bool ProcessLoopbackCapture::isProcessAlive (std::uint32_t pid)
{
    if (pid == 0)
        return false;

    HANDLE h = OpenProcess (SYNCHRONIZE, FALSE, (DWORD) pid);
    if (h == nullptr)
        return false;

    const bool alive = WaitForSingleObject (h, 0) == WAIT_TIMEOUT;
    CloseHandle (h);
    return alive;
}

// ---------------------------------------------------------------------------
// Ciclo de vida
// ---------------------------------------------------------------------------

ProcessLoopbackCapture::ProcessLoopbackCapture (std::uint32_t pid, double sr)
    : juce::Thread ("Backbeat captura do Chrome"),
      processId (pid),
      sampleRate (sr),
      stream (juce::jmax (8192, (int) (sr * 2.0))) // 2 s de folga
{
    stopEvent = CreateEventW (nullptr, TRUE, FALSE, nullptr);
}

ProcessLoopbackCapture::StartResult ProcessLoopbackCapture::start (std::uint32_t pid, double sr)
{
    StartResult result;

    if (pid == 0 || sr <= 0.0)
    {
        result.error = L"processo ou taxa inválidos";
        return result;
    }

    auto capture = std::unique_ptr<ProcessLoopbackCapture> (new ProcessLoopbackCapture (pid, sr));
    capture->startThread (juce::Thread::Priority::high);

    if (! capture->setupDone.wait (3000))
    {
        result.error = L"o Windows não respondeu à ativação da captura";
        return result; // destrutor encerra a thread
    }

    if (capture->setupError.isNotEmpty())
    {
        result.error = capture->setupError;
        return result;
    }

    result.capture = std::move (capture);
    return result;
}

ProcessLoopbackCapture::~ProcessLoopbackCapture()
{
    signalThreadShouldExit();
    if (stopEvent != nullptr)
        SetEvent ((HANDLE) stopEvent);
    stopThread (3000);
    if (stopEvent != nullptr)
        CloseHandle ((HANDLE) stopEvent);
}

juce::String ProcessLoopbackCapture::getFailure() const
{
    const juce::ScopedLock sl (failureLock);
    return failure;
}

double ProcessLoopbackCapture::getSecondsSinceLastSound() const noexcept
{
    const double last = lastSoundMs.load();
    return last <= 0.0 ? 1.0e9 : (juce::Time::getMillisecondCounterHiRes() - last) / 1000.0;
}

// ---------------------------------------------------------------------------
// Thread de captura
// ---------------------------------------------------------------------------

void ProcessLoopbackCapture::run()
{
    const HRESULT coInit = CoInitializeEx (nullptr, COINIT_MULTITHREADED);

    const auto error = setUpAndCapture();
    if (error.isNotEmpty())
    {
        if (! setupSignalled)
        {
            setupError = error;
            setupSignalled = true;
            setupDone.signal();
        }
        else
        {
            const juce::ScopedLock sl (failureLock);
            failure = error;
            failed.store (true);
        }
    }

    running.store (false);
    if (SUCCEEDED (coInit))
        CoUninitialize();
}

juce::String ProcessLoopbackCapture::setUpAndCapture()
{
    // 1. Ativa o dispositivo virtual de loopback do processo (e filhos).
    AUDIOCLIENT_ACTIVATION_PARAMS params {};
    params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    params.ProcessLoopbackParams.TargetProcessId = (DWORD) processId;
    params.ProcessLoopbackParams.ProcessLoopbackMode = PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;

    PROPVARIANT activation {};
    activation.vt = VT_BLOB;
    activation.blob.cbSize = sizeof (params);
    activation.blob.pBlobData = reinterpret_cast<BYTE*> (&params);

    auto* handler = new ActivationHandler();
    ComPtr<IActivateAudioInterfaceAsyncOperation> operation;
    HRESULT hr = ActivateAudioInterfaceAsync (VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof (IAudioClient),
                                              &activation, handler, &operation);
    if (FAILED (hr))
    {
        handler->Release();
        return L"ativação falhou: " + hresultText (hr);
    }

    WaitForSingleObject (handler->done, 3000);
    ComPtr<IAudioClient> client;
    client.p = handler->client;
    hr = handler->result;
    handler->Release();

    if (FAILED (hr) || ! client)
        return L"ativação falhou: " + hresultText (hr);

    // 2. Formato: float estéreo na taxa do ASIO (com fallback para 16 bits).
    bool asFloat = true;
    auto format = makeFormat (sampleRate, true);
    const DWORD flags = AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK
                      | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
    const REFERENCE_TIME bufferDuration = 2'000'000; // 200 ms (unidades de 100 ns)

    hr = client->Initialize (AUDCLNT_SHAREMODE_SHARED, flags, bufferDuration, 0, &format, nullptr);
    if (FAILED (hr))
    {
        asFloat = false;
        format = makeFormat (sampleRate, false);
        hr = client->Initialize (AUDCLNT_SHAREMODE_SHARED, flags, bufferDuration, 0, &format, nullptr);
    }
    if (FAILED (hr))
        return L"não foi possível iniciar a captura: " + hresultText (hr);

    HANDLE packetEvent = CreateEventW (nullptr, FALSE, FALSE, nullptr);
    hr = client->SetEventHandle (packetEvent);

    ComPtr<IAudioCaptureClient> capture;
    if (SUCCEEDED (hr))
        hr = client->GetService (__uuidof (IAudioCaptureClient), reinterpret_cast<void**> (&capture));
    if (SUCCEEDED (hr))
        hr = client->Start();

    if (FAILED (hr))
    {
        CloseHandle (packetEvent);
        return L"não foi possível iniciar a captura: " + hresultText (hr);
    }

    // 3. Pronto: avisa quem chamou start() e entra no laço de captura.
    running.store (true);
    setupSignalled = true;
    setupDone.signal();

    std::vector<float> convert ((size_t) sampleRate * 2); // para o fallback de 16 bits
    const HANDLE waits[] = { (HANDLE) stopEvent, packetEvent };
    juce::String error;

    while (! threadShouldExit())
    {
        const DWORD w = WaitForMultipleObjects (2, waits, FALSE, 200);
        if (w == WAIT_OBJECT_0)
            break;

        UINT32 packetFrames = 0;
        while (SUCCEEDED (hr = capture->GetNextPacketSize (&packetFrames)) && packetFrames > 0)
        {
            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD bufferFlags = 0;

            hr = capture->GetBuffer (&data, &frames, &bufferFlags, nullptr, nullptr);
            if (FAILED (hr))
                break;

            if ((bufferFlags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) != 0)
                discontinuities.fetch_add (1);

            if ((bufferFlags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 || data == nullptr)
            {
                stream.pushSilence ((int) frames);
            }
            else if (asFloat)
            {
                stream.pushInterleaved (reinterpret_cast<const float*> (data), (int) frames, 2);
                lastSoundMs.store (juce::Time::getMillisecondCounterHiRes());
            }
            else
            {
                const auto* pcm = reinterpret_cast<const std::int16_t*> (data);
                const size_t count = juce::jmin ((size_t) frames * 2, convert.size());
                for (size_t i = 0; i < count; ++i)
                    convert[i] = pcm[i] / 32768.0f;
                stream.pushInterleaved (convert.data(), (int) (count / 2), 2);
                lastSoundMs.store (juce::Time::getMillisecondCounterHiRes());
            }

            capture->ReleaseBuffer (frames);
        }

        if (FAILED (hr))
        {
            error = L"a captura parou: " + hresultText (hr);
            break;
        }
    }

    client->Stop();
    CloseHandle (packetEvent);
    return error;
}

} // namespace bb
