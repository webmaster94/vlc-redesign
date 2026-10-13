/* Copyright 2026 webmaster94. GPL-3.0-or-later.
 * Windows audio-focus behavior from Juxtopposed's VLC Redesign, CC BY 4.0. */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "redesign.hpp"
#include "input_manager.hpp"
#include <QSettings>
#include <QTimer>
#include <QDateTime>
#include <vlc_input.h>
#ifdef _WIN32
#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
namespace
{
// The pinned MinGW SDK only forward-declares IAudioMeterInformation.
// ABI and IID match Microsoft's endpointvolume.h; no SDK implementation is copied.
struct AudioMeter : IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetPeakValue(float *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMeteringChannelCount(UINT *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetChannelsPeakValues(UINT32, float *) = 0;
    virtual HRESULT STDMETHODCALLTYPE QueryHardwareSupport(DWORD *) = 0;
};
const GUID audioMeterId = {0xc02216f6, 0x8c67, 0x4b5b, {0x9d, 0x00, 0xd0, 0x08, 0xe7, 0x3e, 0x00, 0x64}};
bool otherAudioActive()
{
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IMMDeviceEnumerator *devices = nullptr;
    bool active = false;
    if (SUCCEEDED(CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_IMMDeviceEnumerator, reinterpret_cast<void **>(&devices))))
    {
        IMMDevice *device = nullptr;
        if (SUCCEEDED(devices->GetDefaultAudioEndpoint(eRender, eMultimedia, &device)))
        {
            IAudioSessionManager2 *manager = nullptr;
            if (SUCCEEDED(device->Activate(IID_IAudioSessionManager2, CLSCTX_INPROC_SERVER, nullptr,
                                           reinterpret_cast<void **>(&manager))))
            {
                IAudioSessionEnumerator *sessions = nullptr;
                if (SUCCEEDED(manager->GetSessionEnumerator(&sessions)))
                {
                    int count = 0;
                    sessions->GetCount(&count);
                    for (int i = 0; i < count && !active; ++i)
                    {
                        IAudioSessionControl *session = nullptr;
                        if (FAILED(sessions->GetSession(i, &session)))
                            continue;
                        IAudioSessionControl2 *details = nullptr;
                        if (SUCCEEDED(session->QueryInterface(IID_IAudioSessionControl2,
                                                              reinterpret_cast<void **>(&details))))
                        {
                            DWORD pid = GetCurrentProcessId();
                            AudioSessionState state = AudioSessionStateInactive;
                            details->GetProcessId(&pid);
                            details->GetState(&state);
                            if (pid != GetCurrentProcessId() && state == AudioSessionStateActive)
                            {
                                AudioMeter *meter = nullptr;
                                ISimpleAudioVolume *volume = nullptr;
                                float peak = 0, level = 0;
                                BOOL muted = TRUE;
                                if (SUCCEEDED(session->QueryInterface(audioMeterId,
                                                                      reinterpret_cast<void **>(&meter))))
                                {
                                    meter->GetPeakValue(&peak);
                                    meter->Release();
                                }
                                if (SUCCEEDED(session->QueryInterface(IID_ISimpleAudioVolume,
                                                                      reinterpret_cast<void **>(&volume))))
                                {
                                    volume->GetMasterVolume(&level);
                                    volume->GetMute(&muted);
                                    volume->Release();
                                }
                                active = !muted && level > 0 && peak > .0001f;
                            }
                            details->Release();
                        }
                        session->Release();
                    }
                    sessions->Release();
                }
                manager->Release();
            }
            device->Release();
        }
        devices->Release();
    }
    if (SUCCEEDED(initialized))
        CoUninitialize();
    return active;
}
} // namespace
#endif
void RedesignShell::startAudioFocus()
{
#ifdef _WIN32
    auto timer = new QTimer(this);
    timer->setInterval(500);
    connect(timer, &QTimer::timeout, this,
            [this]
            {
                auto input = THEMIM->getInput();
                if (!input || !getSettings()->value("Redesign/pauseOtherAudio", false).toBool())
                {
                    setProperty("audioFocusPaused", false);
                    return;
                }
                const bool active = otherAudioActive();
                const qint64 now = QDateTime::currentMSecsSinceEpoch();
                if (active)
                    setProperty("lastOtherAudio", now);
                const int state = var_GetInteger(input, "state");
                if (active && state == PLAYING_S)
                {
                    var_SetInteger(input, "state", PAUSE_S);
                    setProperty("audioFocusPaused", true);
                }
                else if (!active && property("audioFocusPaused").toBool() &&
                         now - property("lastOtherAudio").toLongLong() > 1500)
                {
                    if (state == PAUSE_S)
                        var_SetInteger(input, "state", PLAYING_S);
                    setProperty("audioFocusPaused", false);
                }
            });
    timer->start();
#endif
}
