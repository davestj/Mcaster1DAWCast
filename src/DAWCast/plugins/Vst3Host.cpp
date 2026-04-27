// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Vst3Host.h"

#include <QDebug>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QWidget>
#include <QPushButton>
#include <QByteArray>
#include <QDataStream>
#include <QtEndian>
#include <QTimer>
#include <vector>
#include <cstring>

#ifdef HAVE_VST3
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/common/memorystream.h"
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivsthostapplication.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include "pluginterfaces/vst/vsttypes.h"
#include <string>
#endif

#ifdef __APPLE__
#include <QWindow>
#endif

namespace dawcast::plugins {

Vst3BundleInfo inspectVst3Bundle(const QString& path)
{
    Vst3BundleInfo out;
    out.path = path;

#ifdef HAVE_VST3
    std::string err;
    auto mod = VST3::Hosting::Module::create(path.toStdString(), err);
    if (!mod) {
        out.error = QString::fromStdString(err);
        if (out.error.isEmpty())
            out.error = QStringLiteral("Failed to load VST3 bundle");
        return out;
    }

    out.name = QString::fromStdString(mod->getName());
    out.isBundle = mod->isBundle();

    const auto& factory = mod->getFactory();
    const auto factoryInfo = factory.info();
    const QString vendor = QString::fromStdString(factoryInfo.vendor());

    for (const auto& cls : factory.classInfos()) {
        Vst3ClassInfo info;
        info.name          = QString::fromStdString(cls.name());
        info.category      = QString::fromStdString(cls.category());
        info.subCategories = QString::fromStdString(cls.subCategoriesString());
        info.vendor        = QString::fromStdString(cls.vendor());
        if (info.vendor.isEmpty()) info.vendor = vendor;
        info.version       = QString::fromStdString(cls.version());
        info.sdkVersion    = QString::fromStdString(cls.sdkVersion());

        const auto& uid = cls.ID();
        char hex[33] = {0};
        const auto data = uid.data();
        static const char* H = "0123456789ABCDEF";
        for (int i = 0; i < 16; ++i) {
            hex[i * 2]     = H[(data[i] >> 4) & 0xF];
            hex[i * 2 + 1] = H[ data[i]       & 0xF];
        }
        info.cid = QString::fromLatin1(hex, 32);

        if (info.category.contains(QLatin1String("Audio Module"), Qt::CaseInsensitive)) {
            out.classes.append(info);
        }
    }
#else
    out.error = QStringLiteral("VST3 SDK not compiled in");
#endif

    return out;
}

#ifdef HAVE_VST3

namespace {
    QString g_lastError;

    // Parse the 32-char hex CID back into a 16-byte TUID.
    bool parseCid(const QString& hex, Steinberg::TUID& out) {
        if (hex.size() != 32) return false;
        for (int i = 0; i < 16; ++i) {
            bool ok1 = false, ok2 = false;
            int hi = hex.mid(i * 2, 1).toInt(&ok1, 16);
            int lo = hex.mid(i * 2 + 1, 1).toInt(&ok2, 16);
            if (!ok1 || !ok2) return false;
            out[i] = static_cast<char>((hi << 4) | lo);
        }
        return true;
    }

    // Convert a VST3 String128 (UTF-16 char16_t) into a QString.
    QString s128ToQString(const Steinberg::Vst::String128& s) {
        int len = 0;
        while (len < 128 && s[len] != 0) ++len;
        return QString::fromUtf16(reinterpret_cast<const char16_t*>(s), len);
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Forward-declared NSView bridge helper (implemented in Vst3HostMac.mm).
// On macOS we need to bounce through Obj-C to pull the NSView* out of a
// QWidget and to create a container NSView that the plugin can attach to.
// ────────────────────────────────────────────────────────────────────────────
#ifdef __APPLE__
void* vst3ObtainNSViewForWidget(QWidget* widget);
void  vst3ResizeNSView(void* nsView, int width, int height);
#endif

// Platform-agnostic wrapper: returns an opaque platform handle suitable
// for IPlugView::attached(), plus the right FIDString type.
static void* platformHandleForWidget(QWidget* w, Steinberg::FIDString& typeOut)
{
    if (!w) return nullptr;
#ifdef __APPLE__
    typeOut = Steinberg::kPlatformTypeNSView;
    return vst3ObtainNSViewForWidget(w);
#elif defined(_WIN32)
    typeOut = Steinberg::kPlatformTypeHWND;
    return reinterpret_cast<void*>(w->winId());
#else
    typeOut = Steinberg::kPlatformTypeX11EmbedWindowID;
    return reinterpret_cast<void*>(w->winId());
#endif
}

static void platformResize(void* handle, int w, int h)
{
#ifdef __APPLE__
    if (handle) vst3ResizeNSView(handle, w, h);
#else
    (void)handle; (void)w; (void)h;
#endif
}

// ────────────────────────────────────────────────────────────────────────────
// ComponentHandler — relays plugin->host parameter edits back into our
// parameter-change queue so automation write + GUI sync works. We ignore
// restartComponent requests beyond a debug log; parameter-value-change
// restart flag is tolerated silently (the next getParameterNormalized
// refresh will pick up the new value).
// ────────────────────────────────────────────────────────────────────────────
struct Vst3PluginInstance;

class ComponentHandler : public Steinberg::Vst::IComponentHandler {
public:
    explicit ComponentHandler(Vst3PluginInstance::Impl* owner) : m_owner(owner) {}

    Steinberg::tresult PLUGIN_API beginEdit(Steinberg::Vst::ParamID) override  { return Steinberg::kResultOk; }
    Steinberg::tresult PLUGIN_API endEdit  (Steinberg::Vst::ParamID) override  { return Steinberg::kResultOk; }
    Steinberg::tresult PLUGIN_API performEdit(Steinberg::Vst::ParamID id,
                                              Steinberg::Vst::ParamValue value) override;
    Steinberg::tresult PLUGIN_API restartComponent(Steinberg::int32 /*flags*/) override {
        return Steinberg::kResultOk;
    }

    // FUnknown boilerplate
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** obj) override {
        if (!obj) return Steinberg::kInvalidArgument;
        if (std::memcmp(iid, Steinberg::Vst::IComponentHandler::iid, 16) == 0 ||
            std::memcmp(iid, Steinberg::FUnknown::iid, 16) == 0) {
            *obj = static_cast<Steinberg::Vst::IComponentHandler*>(this);
            addRef();
            return Steinberg::kResultOk;
        }
        *obj = nullptr;
        return Steinberg::kNoInterface;
    }
    Steinberg::uint32 PLUGIN_API addRef()  override { return ++m_ref; }
    Steinberg::uint32 PLUGIN_API release() override {
        Steinberg::uint32 n = --m_ref;
        if (n == 0) delete this;
        return n;
    }

private:
    Vst3PluginInstance::Impl* m_owner = nullptr;
    Steinberg::uint32         m_ref   = 1;
};

// ────────────────────────────────────────────────────────────────────────────
// PlugFrame — minimal implementation that just acknowledges resizeView
// and resizes our platform container so the plugin editor lays out
// correctly when it asks the host for more room.
// ────────────────────────────────────────────────────────────────────────────
class PlugFrame : public Steinberg::IPlugFrame {
public:
    explicit PlugFrame(QWidget* container) : m_container(container) {}
    void setPlatformHandle(void* h) { m_handle = h; }

    Steinberg::tresult PLUGIN_API resizeView(Steinberg::IPlugView* view,
                                             Steinberg::ViewRect*  newSize) override {
        if (!view || !newSize || !m_container) return Steinberg::kInvalidArgument;
        const int w = newSize->getWidth();
        const int h = newSize->getHeight();
        if (m_container) {
            m_container->setFixedSize(w, h);
        }
        platformResize(m_handle, w, h);
        view->onSize(newSize);
        return Steinberg::kResultOk;
    }

    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** obj) override {
        if (!obj) return Steinberg::kInvalidArgument;
        if (std::memcmp(iid, Steinberg::IPlugFrame::iid, 16) == 0 ||
            std::memcmp(iid, Steinberg::FUnknown::iid,    16) == 0) {
            *obj = static_cast<Steinberg::IPlugFrame*>(this);
            addRef();
            return Steinberg::kResultOk;
        }
        *obj = nullptr;
        return Steinberg::kNoInterface;
    }
    Steinberg::uint32 PLUGIN_API addRef()  override { return ++m_ref; }
    Steinberg::uint32 PLUGIN_API release() override {
        Steinberg::uint32 n = --m_ref;
        if (n == 0) delete this;
        return n;
    }

private:
    QWidget*          m_container = nullptr;
    void*             m_handle    = nullptr;
    Steinberg::uint32 m_ref       = 1;
};

// ────────────────────────────────────────────────────────────────────────────
// Impl
// ────────────────────────────────────────────────────────────────────────────
struct Vst3PluginInstance::Impl {
    VST3::Hosting::Module::Ptr                           module;
    Steinberg::IPtr<Steinberg::Vst::IComponent>          component;
    Steinberg::IPtr<Steinberg::Vst::IAudioProcessor>     processor;
    Steinberg::IPtr<Steinberg::Vst::IEditController>     controller;
    Steinberg::IPtr<Steinberg::Vst::IHostApplication>    host;

    // Lock-free GUI→audio parameter change ring. Drained into
    // inputChanges each block.
    std::unique_ptr<Steinberg::Vst::ParameterChangeTransfer> paramTransfer;
    Steinberg::Vst::ParameterChanges                         inputChanges;
    Steinberg::Vst::ParameterChanges                         outputChanges;

    // Cached parameter list (gathered once at create() time so the GUI
    // thread doesn't have to poke the controller for every paint).
    QList<Vst3ParameterInfo> params;

    // Preallocated deinterleaved channel buffers for RT-safe process().
    std::vector<float> bufL;
    std::vector<float> bufR;
    float*             channelPtrs[2] = { nullptr, nullptr };
    int                maxFrames      = 0;
    QString            name;
    bool               active         = false;
    bool               processing     = false;
};

Steinberg::tresult PLUGIN_API ComponentHandler::performEdit(
    Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue value)
{
    if (m_owner && m_owner->paramTransfer) {
        m_owner->paramTransfer->addChange(id, value, 0);
    }
    return Steinberg::kResultOk;
}

// ────────────────────────────────────────────────────────────────────────────
// Helpers
// ────────────────────────────────────────────────────────────────────────────
static void gatherParameters(Vst3PluginInstance::Impl* impl)
{
    using namespace Steinberg;
    using namespace Steinberg::Vst;

    impl->params.clear();
    if (!impl->controller) return;

    const int32 n = impl->controller->getParameterCount();
    impl->params.reserve(n);
    for (int32 i = 0; i < n; ++i) {
        ParameterInfo pi{};
        if (impl->controller->getParameterInfo(i, pi) != kResultOk) continue;

        Vst3ParameterInfo info;
        info.id                 = pi.id;
        info.title              = s128ToQString(pi.title);
        info.shortTitle         = s128ToQString(pi.shortTitle);
        info.units              = s128ToQString(pi.units);
        info.defaultNormalized  = pi.defaultNormalizedValue;
        info.stepCount          = pi.stepCount;
        info.canAutomate        = (pi.flags & ParameterInfo::kCanAutomate)  != 0;
        info.readOnly           = (pi.flags & ParameterInfo::kIsReadOnly)   != 0;
        info.isBypass           = (pi.flags & ParameterInfo::kIsBypass)     != 0;
        impl->params.append(info);
    }
}

// ────────────────────────────────────────────────────────────────────────────
// create()
// ────────────────────────────────────────────────────────────────────────────
std::unique_ptr<Vst3PluginInstance> Vst3PluginInstance::create(
    const QString& bundlePath, const QString& cidHex,
    double sampleRate, int maxFrames)
{
    using namespace Steinberg;
    using namespace Steinberg::Vst;

    g_lastError.clear();

    std::string err;
    auto mod = VST3::Hosting::Module::create(bundlePath.toStdString(), err);
    if (!mod) {
        g_lastError = QStringLiteral("Module::create: %1").arg(QString::fromStdString(err));
        return nullptr;
    }

    TUID cid{};
    if (!parseCid(cidHex, cid)) {
        g_lastError = QStringLiteral("Invalid class id: %1").arg(cidHex);
        return nullptr;
    }

    const auto& factory = mod->getFactory();
    IPtr<IComponent> component;
    VST3::UID componentUid;
    for (const auto& cls : factory.classInfos()) {
        if (std::memcmp(cls.ID().data(), cid, 16) == 0) {
            componentUid = cls.ID();
            component = factory.createInstance<IComponent>(cls.ID());
            break;
        }
    }
    if (!component) {
        g_lastError = QStringLiteral("Class not found: %1").arg(cidHex);
        return nullptr;
    }

    // Minimal host context — SDK-provided HostApplication.
    IPtr<IHostApplication> host = owned(new HostApplication());

    if (component->initialize(host) != kResultOk) {
        g_lastError = QStringLiteral("IComponent::initialize failed");
        return nullptr;
    }

    IAudioProcessor* rawProc = nullptr;
    if (component->queryInterface(IAudioProcessor::iid,
                                  reinterpret_cast<void**>(&rawProc))
        != kResultOk || !rawProc) {
        component->terminate();
        g_lastError = QStringLiteral("Plugin has no IAudioProcessor");
        return nullptr;
    }
    IPtr<IAudioProcessor> processor = owned(rawProc);

    // ── IEditController discovery ──────────────────────────────────────
    // Try the unified (single-class) path first: the component already
    // implements IEditController.
    IPtr<IEditController> controller;
    {
        IEditController* rawCtrl = nullptr;
        if (component->queryInterface(IEditController::iid,
                                      reinterpret_cast<void**>(&rawCtrl))
            == kResultOk && rawCtrl) {
            controller = owned(rawCtrl);
        }
    }
    // Separate controller — look up getControllerClassId + factory->createInstance.
    if (!controller) {
        TUID ctrlCid{};
        if (component->getControllerClassId(ctrlCid) == kResultOk) {
            for (const auto& cls : factory.classInfos()) {
                if (std::memcmp(cls.ID().data(), ctrlCid, 16) == 0) {
                    controller = factory.createInstance<IEditController>(cls.ID());
                    break;
                }
            }
            // Fallback: try createInstance directly with the raw UID
            // — some SDK builds don't surface controller-only classes
            // via classInfos() (they're tagged as "Controller Class").
            if (!controller) {
                VST3::UID ctrlUid = VST3::UID::fromTUID(ctrlCid);
                controller = factory.createInstance<IEditController>(ctrlUid);
            }
            if (controller) {
                if (controller->initialize(host) != kResultOk) {
                    qWarning() << "VST3: IEditController::initialize failed — continuing "
                                  "without controller";
                    controller = nullptr;
                }
            }
        }
    }

    // Sync component state into the controller so defaults match.
    if (controller) {
        MemoryStream stream;
        if (component->getState(&stream) == kResultOk) {
            stream.seek(0, IBStream::kIBSeekSet, nullptr);
            controller->setComponentState(&stream);
        }
    }

    if (processor->canProcessSampleSize(kSample32) != kResultOk) {
        if (controller) controller->terminate();
        component->terminate();
        g_lastError = QStringLiteral("Plugin does not support 32-bit float processing");
        return nullptr;
    }

    SpeakerArrangement sa = SpeakerArr::kStereo;
    if (processor->setBusArrangements(&sa, 1, &sa, 1) != kResultOk) {
        qWarning() << "VST3: setBusArrangements(stereo,stereo) not accepted — trying anyway";
    }

    const int32 inCount  = component->getBusCount(kAudio, kInput);
    const int32 outCount = component->getBusCount(kAudio, kOutput);
    for (int32 i = 0; i < inCount;  ++i) component->activateBus(kAudio, kInput,  i, true);
    for (int32 i = 0; i < outCount; ++i) component->activateBus(kAudio, kOutput, i, true);

    ProcessSetup setup{};
    setup.processMode        = kRealtime;
    setup.symbolicSampleSize = kSample32;
    setup.maxSamplesPerBlock = maxFrames;
    setup.sampleRate         = sampleRate;
    if (processor->setupProcessing(setup) != kResultOk) {
        if (controller) controller->terminate();
        component->terminate();
        g_lastError = QStringLiteral("setupProcessing failed");
        return nullptr;
    }

    if (component->setActive(true) != kResultOk) {
        if (controller) controller->terminate();
        component->terminate();
        g_lastError = QStringLiteral("setActive failed");
        return nullptr;
    }
    processor->setProcessing(true);

    auto inst = std::unique_ptr<Vst3PluginInstance>(new Vst3PluginInstance());
    inst->m_impl->module     = mod;
    inst->m_impl->component  = component;
    inst->m_impl->processor  = processor;
    inst->m_impl->controller = controller;
    inst->m_impl->host       = host;
    inst->m_impl->maxFrames  = maxFrames;
    inst->m_impl->bufL.assign(static_cast<size_t>(maxFrames), 0.0f);
    inst->m_impl->bufR.assign(static_cast<size_t>(maxFrames), 0.0f);
    inst->m_impl->channelPtrs[0] = inst->m_impl->bufL.data();
    inst->m_impl->channelPtrs[1] = inst->m_impl->bufR.data();
    inst->m_impl->active     = true;
    inst->m_impl->processing = true;
    inst->m_impl->name       = QString::fromStdString(mod->getName());

    // Gather parameter metadata up front.
    gatherParameters(inst->m_impl.get());

    // Pre-size input change ring to the number of parameters + a little
    // headroom so rapid bursts don't drop edits.
    const int pCount = static_cast<int>(inst->m_impl->params.size());
    const int nChangesCap = std::max(64, pCount * 4);
    inst->m_impl->paramTransfer.reset(
        new ParameterChangeTransfer(nChangesCap));
    inst->m_impl->inputChanges.setMaxParameters(std::max(1, pCount));
    inst->m_impl->outputChanges.setMaxParameters(std::max(1, pCount));

    // Install a component handler so the plugin's own editor can push
    // parameter edits back to us. Controller owns the handler pointer —
    // we release our ref after handing it over.
    if (inst->m_impl->controller) {
        auto* handler = new ComponentHandler(inst->m_impl.get());
        inst->m_impl->controller->setComponentHandler(handler);
        handler->release(); // controller adds its own ref
    }

    return inst;
}

Vst3PluginInstance::Vst3PluginInstance()
    : m_impl(std::make_unique<Impl>())
{
}

Vst3PluginInstance::~Vst3PluginInstance()
{
    if (!m_impl) return;
    if (m_impl->processing && m_impl->processor) {
        m_impl->processor->setProcessing(false);
    }
    if (m_impl->active && m_impl->component) {
        m_impl->component->setActive(false);
    }
    if (m_impl->controller) {
        m_impl->controller->setComponentHandler(nullptr);
        m_impl->controller->terminate();
        m_impl->controller = nullptr;
    }
    if (m_impl->component) {
        m_impl->component->terminate();
    }
    // Release in order — processor, then component.
    m_impl->processor = nullptr;
    m_impl->component = nullptr;
    m_impl->host      = nullptr;
    m_impl->paramTransfer.reset();
    m_impl->module.reset();
}

QString Vst3PluginInstance::displayName() const
{
    return m_impl ? m_impl->name : QString();
}

// ────────────────────────────────────────────────────────────────────────────
// process()
// ────────────────────────────────────────────────────────────────────────────
void Vst3PluginInstance::process(float* buffer, int frames, int channels)
{
    using namespace Steinberg;
    using namespace Steinberg::Vst;

    if (!m_impl || !m_impl->processor || !m_impl->processing) return;
    if (!buffer || frames <= 0 || channels != 2) return;
    if (frames > m_impl->maxFrames) {
        int offset = 0;
        while (offset < frames) {
            int chunk = std::min(m_impl->maxFrames, frames - offset);
            process(buffer + offset * channels, chunk, channels);
            offset += chunk;
        }
        return;
    }

    // Deinterleave into per-channel buffers.
    for (int f = 0; f < frames; ++f) {
        m_impl->bufL[f] = buffer[f * 2    ];
        m_impl->bufR[f] = buffer[f * 2 + 1];
    }

    AudioBusBuffers inBus{};
    inBus.numChannels       = 2;
    inBus.silenceFlags      = 0;
    inBus.channelBuffers32  = m_impl->channelPtrs;

    AudioBusBuffers outBus{};
    outBus.numChannels      = 2;
    outBus.silenceFlags     = 0;
    outBus.channelBuffers32 = m_impl->channelPtrs; // in-place

    // Drain pending parameter changes into the block's input queue.
    m_impl->inputChanges.clearQueue();
    m_impl->outputChanges.clearQueue();
    if (m_impl->paramTransfer) {
        m_impl->paramTransfer->transferChangesTo(m_impl->inputChanges);
    }

    ProcessData data{};
    data.processMode         = kRealtime;
    data.symbolicSampleSize  = kSample32;
    data.numSamples          = frames;
    data.numInputs           = 1;
    data.numOutputs          = 1;
    data.inputs              = &inBus;
    data.outputs             = &outBus;
    data.inputParameterChanges  = &m_impl->inputChanges;
    data.outputParameterChanges = &m_impl->outputChanges;

    tresult r = m_impl->processor->process(data);
    if (r != kResultOk) {
        qWarning() << "VST3 process() returned" << r;
        return;
    }

    // Reinterleave result back into the caller's buffer.
    for (int f = 0; f < frames; ++f) {
        buffer[f * 2    ] = m_impl->bufL[f];
        buffer[f * 2 + 1] = m_impl->bufR[f];
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Parameters (GUI-thread only)
// ────────────────────────────────────────────────────────────────────────────
int Vst3PluginInstance::parameterCount() const
{
    return m_impl ? m_impl->params.size() : 0;
}

Vst3ParameterInfo Vst3PluginInstance::parameterInfo(int index) const
{
    if (!m_impl || index < 0 || index >= m_impl->params.size())
        return Vst3ParameterInfo{};
    return m_impl->params.at(index);
}

double Vst3PluginInstance::getParameterNormalized(int index) const
{
    if (!m_impl || !m_impl->controller) return 0.0;
    if (index < 0 || index >= m_impl->params.size()) return 0.0;
    return m_impl->controller->getParamNormalized(m_impl->params.at(index).id);
}

void Vst3PluginInstance::setParameterNormalized(int index, double value)
{
    if (!m_impl || index < 0 || index >= m_impl->params.size()) return;
    if (value < 0.0) value = 0.0;
    if (value > 1.0) value = 1.0;

    const auto pid = m_impl->params.at(index).id;

    // Update controller immediately so the editor UI reflects the value
    // even if the audio thread is idle.
    if (m_impl->controller) {
        m_impl->controller->setParamNormalized(pid, value);
    }
    // Push into the lock-free ring so the next audio block picks it up.
    if (m_impl->paramTransfer) {
        m_impl->paramTransfer->addChange(pid, value, 0);
    }
}

// ────────────────────────────────────────────────────────────────────────────
// openEditor() — native IPlugView if available, fallback generic dialog.
// ────────────────────────────────────────────────────────────────────────────
namespace {

QDialog* buildGenericEditor(Vst3PluginInstance* inst, QWidget* parent)
{
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(QObject::tr("%1 — Parameters").arg(inst->displayName()));

    auto* scroll   = new QScrollArea(dlg);
    scroll->setWidgetResizable(true);
    auto* inner    = new QWidget(scroll);
    auto* form     = new QFormLayout(inner);

    const int n = inst->parameterCount();
    if (n == 0) {
        form->addRow(new QLabel(QObject::tr("This plugin exposes no parameters."), inner));
    }

    for (int i = 0; i < n; ++i) {
        const auto pi = inst->parameterInfo(i);
        if (pi.readOnly) continue;

        auto* row    = new QWidget(inner);
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 0, 0, 0);

        auto* slider = new QSlider(Qt::Horizontal, row);
        slider->setRange(0, 10000);
        const double cur = inst->getParameterNormalized(i);
        slider->setValue(static_cast<int>(cur * 10000.0));
        slider->setToolTip(QObject::tr("Double-click to reset to default (%1)")
                               .arg(pi.defaultNormalized, 0, 'f', 3));

        auto* value = new QLabel(QString::number(cur, 'f', 3), row);
        value->setFixedWidth(56);

        QObject::connect(slider, &QSlider::valueChanged, dlg,
                         [inst, i, value](int v) {
            double norm = v / 10000.0;
            inst->setParameterNormalized(i, norm);
            value->setText(QString::number(norm, 'f', 3));
        });

        // Double-click the slider to reset — QSlider emits no such event,
        // so we add a small "R" reset button next to it.
        auto* resetBtn = new QPushButton(QObject::tr("R"), row);
        resetBtn->setFixedWidth(24);
        resetBtn->setToolTip(QObject::tr("Reset to default"));
        QObject::connect(resetBtn, &QPushButton::clicked, dlg,
                         [inst, i, slider, pi]() {
            inst->setParameterNormalized(i, pi.defaultNormalized);
            slider->setValue(static_cast<int>(pi.defaultNormalized * 10000.0));
        });

        rowLay->addWidget(slider, 1);
        rowLay->addWidget(value);
        rowLay->addWidget(resetBtn);

        QString label = pi.title;
        if (!pi.units.isEmpty()) label += QStringLiteral(" (") + pi.units + QStringLiteral(")");
        form->addRow(label, row);
    }

    inner->setLayout(form);
    scroll->setWidget(inner);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::accept);

    auto* layout = new QVBoxLayout(dlg);
    layout->addWidget(scroll, 1);
    layout->addWidget(buttons);
    dlg->resize(420, 520);
    return dlg;
}

} // namespace

QDialog* Vst3PluginInstance::openEditor(QWidget* parent)
{
    using namespace Steinberg;
    using namespace Steinberg::Vst;

    if (!m_impl) return nullptr;

    // No controller -> generic fallback.
    if (!m_impl->controller) {
        return buildGenericEditor(this, parent);
    }

    IPlugView* rawView = m_impl->controller->createView(ViewType::kEditor);
    if (!rawView) {
        return buildGenericEditor(this, parent);
    }
    IPtr<IPlugView> view = owned(rawView);

    // Determine platform type.
    FIDString platformType = kPlatformTypeNSView;
#ifdef _WIN32
    platformType = kPlatformTypeHWND;
#elif !defined(__APPLE__)
    platformType = kPlatformTypeX11EmbedWindowID;
#endif
    if (view->isPlatformTypeSupported(platformType) != kResultTrue) {
        return buildGenericEditor(this, parent);
    }

    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(displayName());

    // Container QWidget whose native handle we hand to the plugin.
    auto* container = new QWidget(dlg);
    container->setAttribute(Qt::WA_NativeWindow);
    container->setAttribute(Qt::WA_DontCreateNativeAncestors);

    auto* layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(container);

    // Query wanted size.
    ViewRect rect{};
    if (view->getSize(&rect) == kResultOk) {
        const int w = rect.getWidth();
        const int h = rect.getHeight();
        if (w > 0 && h > 0) {
            container->setFixedSize(w, h);
            dlg->resize(w, h);
        }
    }

    FIDString platformTypeOut = platformType;
    void* handle = platformHandleForWidget(container, platformTypeOut);
    if (!handle) {
        delete dlg;
        return buildGenericEditor(this, parent);
    }

    // Create a frame so the plugin can request resizes. We let the
    // dialog own it via Qt parenting — PlugFrame is heap, we give it
    // to the controller; we also keep a weak pointer to resize from here.
    auto* frame = new PlugFrame(container);
    frame->setPlatformHandle(handle);
    view->setFrame(frame);

    if (view->attached(handle, platformTypeOut) != kResultOk) {
        view->setFrame(nullptr);
        frame->release();
        delete dlg;
        return buildGenericEditor(this, parent);
    }

    // Keep the view + frame alive for the dialog's lifetime and detach
    // cleanly on close.
    QObject::connect(dlg, &QDialog::finished, dlg, [view, frame, container]() {
        view->removed();
        view->setFrame(nullptr);
        frame->release();
        (void)container;
    });

    return dlg;
}

// ────────────────────────────────────────────────────────────────────────────
// saveState / restoreState
// ────────────────────────────────────────────────────────────────────────────
// Blob layout:
//   magic      : "DCV3"                  (4 bytes)
//   version    : uint32 LE               (4 bytes)
//   compSize   : uint32 LE               (4 bytes)
//   compBlob   : raw bytes               (compSize)
//   ctrlSize   : uint32 LE               (4 bytes)
//   ctrlBlob   : raw bytes               (ctrlSize)
// ────────────────────────────────────────────────────────────────────────────
bool Vst3PluginInstance::saveState(QByteArray& out) const
{
    using namespace Steinberg;

    if (!m_impl || !m_impl->component) return false;

    MemoryStream compStream;
    if (m_impl->component->getState(&compStream) != kResultOk) {
        qWarning() << "VST3 saveState: IComponent::getState failed";
        return false;
    }

    MemoryStream ctrlStream;
    bool haveCtrl = false;
    if (m_impl->controller) {
        if (m_impl->controller->getState(&ctrlStream) == kResultOk) {
            haveCtrl = true;
        }
    }

    out.clear();
    out.append("DCV3", 4);

    auto appendU32 = [&](quint32 v) {
        quint32 le = qToLittleEndian(v);
        out.append(reinterpret_cast<const char*>(&le), 4);
    };

    appendU32(1);                 // format version
    appendU32(static_cast<quint32>(compStream.getSize()));
    if (compStream.getSize() > 0)
        out.append(compStream.getData(), static_cast<int>(compStream.getSize()));

    appendU32(haveCtrl ? static_cast<quint32>(ctrlStream.getSize()) : 0);
    if (haveCtrl && ctrlStream.getSize() > 0)
        out.append(ctrlStream.getData(), static_cast<int>(ctrlStream.getSize()));

    return true;
}

bool Vst3PluginInstance::restoreState(const QByteArray& in)
{
    using namespace Steinberg;

    if (!m_impl || !m_impl->component) return false;
    if (in.size() < 12) return false;
    if (std::memcmp(in.constData(), "DCV3", 4) != 0) return false;

    auto readU32 = [&](int offset) -> quint32 {
        quint32 raw = 0;
        std::memcpy(&raw, in.constData() + offset, 4);
        return qFromLittleEndian(raw);
    };

    const quint32 version  = readU32(4);
    if (version != 1) return false;

    int cursor = 8;
    const quint32 compSize = readU32(cursor);
    cursor += 4;
    if (cursor + static_cast<int>(compSize) > in.size()) return false;
    const char* compData = in.constData() + cursor;
    cursor += static_cast<int>(compSize);

    if (cursor + 4 > in.size()) return false;
    const quint32 ctrlSize = readU32(cursor);
    cursor += 4;
    if (cursor + static_cast<int>(ctrlSize) > in.size()) return false;
    const char* ctrlData = in.constData() + cursor;

    // ── Apply component state ──────────────────────────────────────────
    {
        MemoryStream stream(const_cast<char*>(compData), static_cast<int>(compSize));
        stream.seek(0, IBStream::kIBSeekSet, nullptr);
        if (m_impl->component->setState(&stream) != kResultOk) {
            qWarning() << "VST3 restoreState: IComponent::setState failed";
            return false;
        }
        // Also re-sync the controller with the new component state.
        if (m_impl->controller) {
            stream.seek(0, IBStream::kIBSeekSet, nullptr);
            m_impl->controller->setComponentState(&stream);
        }
    }

    // ── Apply controller state (editor view state) ────────────────────
    if (m_impl->controller && ctrlSize > 0) {
        MemoryStream stream(const_cast<char*>(ctrlData), static_cast<int>(ctrlSize));
        stream.seek(0, IBStream::kIBSeekSet, nullptr);
        if (m_impl->controller->setState(&stream) != kResultOk) {
            qWarning() << "VST3 restoreState: IEditController::setState failed";
        }
    }

    // Refresh our cached parameter list's defaults (IDs stay the same —
    // normalized values are now live inside the controller).
    return true;
}

QString Vst3PluginInstance::lastError()
{
    return g_lastError;
}

#else // !HAVE_VST3

struct Vst3PluginInstance::Impl {};

std::unique_ptr<Vst3PluginInstance> Vst3PluginInstance::create(
    const QString&, const QString&, double, int)
{
    return nullptr;
}
Vst3PluginInstance::Vst3PluginInstance() = default;
Vst3PluginInstance::~Vst3PluginInstance() = default;
QString Vst3PluginInstance::displayName() const { return {}; }
void Vst3PluginInstance::process(float*, int, int) {}
int  Vst3PluginInstance::parameterCount() const { return 0; }
Vst3ParameterInfo Vst3PluginInstance::parameterInfo(int) const { return {}; }
double Vst3PluginInstance::getParameterNormalized(int) const { return 0.0; }
void   Vst3PluginInstance::setParameterNormalized(int, double) {}
QDialog* Vst3PluginInstance::openEditor(QWidget*) { return nullptr; }
bool Vst3PluginInstance::saveState(QByteArray&) const { return false; }
bool Vst3PluginInstance::restoreState(const QByteArray&) { return false; }
QString Vst3PluginInstance::lastError()
{ return QStringLiteral("VST3 SDK not compiled in"); }

#endif

} // namespace dawcast::plugins
