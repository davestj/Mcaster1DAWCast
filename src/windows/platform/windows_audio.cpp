// Mcaster1DAWCast — Windows audio device enumeration (WASAPI)
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "windows_audio.h"
#include "compat_windows.h"

#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <audioclient.h>
#include <propvarutil.h>

namespace {

struct ComInit {
    HRESULT hr;
    ComInit()  { hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED); }
    ~ComInit() { if (SUCCEEDED(hr)) CoUninitialize(); }
};

template <class T>
struct ComPtr {
    T* p = nullptr;
    ~ComPtr() { if (p) p->Release(); }
    T** put() { return &p; }
    T*  get() const { return p; }
    T*  operator->() const { return p; }
    explicit operator bool() const { return p != nullptr; }
};

QString prop_to_qstring(IPropertyStore* store, const PROPERTYKEY& key) {
    PROPVARIANT v; PropVariantInit(&v);
    QString out;
    if (SUCCEEDED(store->GetValue(key, &v)) && v.vt == VT_LPWSTR && v.pwszVal) {
        out = QString::fromWCharArray(v.pwszVal);
    }
    PropVariantClear(&v);
    return out;
}

} // namespace

namespace dawcast {

QList<AudioDeviceInfo> WindowsAudioDevices::enumerate() {
    QList<AudioDeviceInfo> out;
    ComInit com;

    ComPtr<IMMDeviceEnumerator> enumerator;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator),
                                reinterpret_cast<void**>(enumerator.put())))) {
        return out;
    }

    const QString defRender  = defaultRenderDeviceId();
    const QString defCapture = defaultCaptureDeviceId();

    for (EDataFlow flow : { eRender, eCapture }) {
        ComPtr<IMMDeviceCollection> coll;
        if (FAILED(enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, coll.put()))) {
            continue;
        }
        UINT count = 0;
        coll->GetCount(&count);

        for (UINT i = 0; i < count; ++i) {
            ComPtr<IMMDevice> dev;
            if (FAILED(coll->Item(i, dev.put()))) continue;

            LPWSTR rawId = nullptr;
            if (FAILED(dev->GetId(&rawId)) || !rawId) continue;
            const QString id = QString::fromWCharArray(rawId);
            CoTaskMemFree(rawId);

            ComPtr<IPropertyStore> props;
            if (FAILED(dev->OpenPropertyStore(STGM_READ, props.put()))) continue;

            AudioDeviceInfo info;
            info.id         = id;
            info.name       = prop_to_qstring(props.get(), PKEY_Device_FriendlyName);
            info.isInput    = (flow == eCapture);
            info.isOutput   = (flow == eRender);
            info.isDefault  = (flow == eRender)  ? (id == defRender)
                                                  : (id == defCapture);
            info.driverName = QStringLiteral("WASAPI Shared");

            // Query mix format for sample rate + channel count.
            ComPtr<IAudioClient> client;
            if (SUCCEEDED(dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                                        nullptr,
                                        reinterpret_cast<void**>(client.put())))) {
                WAVEFORMATEX* wf = nullptr;
                if (SUCCEEDED(client->GetMixFormat(&wf)) && wf) {
                    info.defaultSampleRate = wf->nSamplesPerSec;
                    info.maxChannels       = wf->nChannels;
                    CoTaskMemFree(wf);
                }
            }

            // Heuristic: AudioPipe virtual devices advertise themselves in the
            // friendly name. Flag for UI grouping.
            info.isLoopback = info.name.contains(QStringLiteral("AudioPipe"), Qt::CaseInsensitive)
                           || info.name.contains(QStringLiteral("Virtual"),   Qt::CaseInsensitive);

            out.append(info);
        }
    }
    return out;
}

QString WindowsAudioDevices::defaultRenderDeviceId() {
    ComInit com;
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator),
                                reinterpret_cast<void**>(enumerator.put())))) return {};
    ComPtr<IMMDevice> dev;
    if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, dev.put()))) return {};
    LPWSTR id = nullptr;
    if (FAILED(dev->GetId(&id)) || !id) return {};
    QString out = QString::fromWCharArray(id);
    CoTaskMemFree(id);
    return out;
}

QString WindowsAudioDevices::defaultCaptureDeviceId() {
    ComInit com;
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator),
                                reinterpret_cast<void**>(enumerator.put())))) return {};
    ComPtr<IMMDevice> dev;
    if (FAILED(enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, dev.put()))) return {};
    LPWSTR id = nullptr;
    if (FAILED(dev->GetId(&id)) || !id) return {};
    QString out = QString::fromWCharArray(id);
    CoTaskMemFree(id);
    return out;
}

} // namespace dawcast
