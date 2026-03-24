#include "audio/AudioEngine.hpp"
#include "audio/library/effects/GainPan.hpp"
#include "audio/library/effects/Distortion.hpp"

#include <audio/dsp/generators/VST3Generator.hpp>
#include <audio/dsp/BuiltInProcessor.hpp>
#include <audio/dsp/VST3Processor.hpp>

#include <algorithm>
#include <cmath>

#include <numeric>
#include <cstring>

#ifndef M_PI
#define M_PI 3.141592653589793238462643383279502884
#endif

using gs::audio::BuiltInProcessor;
using gs::audio::VST3Processor;
using gs::audio::VST3Generator;

static float parabolicInterp(const std::vector<float>& v, int i) {
    float ym1 = (i > 0) ? v[i - 1] : v[i];
    float y0  = v[i];
    float yp1 = (i + 1 < (int)v.size()) ? v[i + 1] : v[i];
    float denom = 2.0f * (2.0f * y0 - ym1 - yp1);
    if (std::fabs(denom) < 1e-12f) return static_cast<float>(i);
    return static_cast<float>(i) + (ym1 - yp1) / denom;
}

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine() {
    shutdown();
}


bool AudioEngine::initialize(const EngineConfig& config) {
    m_config = config;

    m_vst3Host = std::make_unique<VST3Host>();
    if (!m_vst3Host->Initialize(config.device.sampleRate, config.device.blockSize)) {
        LOG_WARN("[AudioEngine] VST3Host init failed (continuing without VST3)");
    }
    m_device = std::make_unique<AudioDeviceIO>();
    if (!m_device->open(config.device)) {
        LOG_ERROR("[AudioEngine] Failed to open audio device.");
        return false;
    }
    m_midiDevice = std::make_unique<MidiDeviceIO>();
    if (!config.midi.deviceName.empty()) {
        if (m_midiDevice->openInput(config.midi.portIndex, config.midi.deviceName))
            m_midiDevice->start();
    } else {
        LOG_INFO("[AudioEngine] No MIDI device configured. Available:");
        MidiDeviceIO::printDevices();
    }
    const int sr    = m_device->getSampleRate();
    const int block = m_device->getBlockSize();

    m_midiStorage.resize(256);
    m_midiScratch.events   = m_midiStorage.data();
    m_midiScratch.count    = 0;
    m_midiScratch.capacity = static_cast<int>(m_midiStorage.size());

    m_outputMeter.setSampleRate(sr);
    m_waveform.resize(sr * 2);

    {
        auto master = std::make_unique<Bus>(kMasterBusId, "Master", 2, block);
        PC prepCtx;
        prepCtx.sampleRate   = static_cast<double>(sr);
        prepCtx.maxBlockSize = block;
        master->prepare(prepCtx);
        m_buses.push_back(std::move(master));
    }

    if (config.createDefaultTrack) {
        InputSource src;
        src.kind = InputSource::Kind::Hardware;
        src.hardwareChannel = 0;
        createTrack("Track 1", 2, src, kMasterBusId);
    }

    m_channelRack = std::make_unique<ChannelRack>();
    m_channelRack->prepare(static_cast<double>(sr), block);

    m_device->setProcessCallback(
        [this](const AudioBuffer& in, AudioBuffer& out, int frames, const PC& ctx) {
            processBlock(in, out, frames, ctx);
        }
    );

    if (!m_device->start()) {
        LOG_ERROR("[AudioEngine] Failed to start audio stream.");
        m_device->close();
        return false;
    }

    LOG_INFO("[AudioEngine] Initialized: %s @ %d Hz, %d frames", m_device->getDeviceName().c_str(), sr, block);
    return true;
}

void AudioEngine::shutdown() {
    if (m_device) {
        m_device->close();
        m_device.reset();
    }
    if (m_midiDevice) {
        m_midiDevice->closeInput();
        m_midiDevice.reset();
    }
    if (m_channelRack) {
        m_channelRack->stop();
        m_channelRack.reset();
    }
    if (m_vst3Host) {
        m_vst3Host->Shutdown();
        m_vst3Host.reset();
    }
    m_tracks.clear();
    m_buses.clear();
}

bool AudioEngine::isRunning() const {
    return m_device && m_device->isRunning();
}

int         AudioEngine::getSampleRate()     const { return m_device ? m_device->getSampleRate()     : 48000; }
int         AudioEngine::getBlockSize()      const { return m_device ? m_device->getBlockSize()      : 256; }
int         AudioEngine::getInputChannels()  const { return m_device ? m_device->getInputChannels()  : 0; }
int         AudioEngine::getOutputChannels() const { return m_device ? m_device->getOutputChannels() : 0; }
std::string AudioEngine::getDeviceName()     const { return m_device ? m_device->getDeviceName()     : ""; }

void AudioEngine::toggleBypass() { m_bypassEnabled.store(!m_bypassEnabled.load()); }
void AudioEngine::setBypass(bool e) { m_bypassEnabled.store(e); }
bool AudioEngine::isBypassed() const { return m_bypassEnabled.load(); }

float AudioEngine::getRmsDb()  const { return m_outputMeter.getRmsDb(); }
float AudioEngine::getPeakDb() const { return m_outputMeter.getPeakDb(); }

void AudioEngine::getRecentWaveform(int numSamples, std::vector<float>& out) const {
    m_waveform.readRecent(numSamples, out);
}

int AudioEngine::getWaveformBufferSize() const { return m_waveform.capacity(); }

void AudioEngine::processBlock(
    const AudioBuffer& input, AudioBuffer& output,
    int frames, const PC& ctx)
{
    // if (input.channels > 0 && input.data) {
    //     m_inputMeter.processSamples(input.channel(0), frames);
    //     m_waveform.write(input.channel(0), frames);
    // }

    if (m_bypassEnabled.load(std::memory_order_relaxed)) {
        output.clear();
        return;
    }

    bool anySoloed = false;
    for (auto& t : m_tracks) {
        if (t->isSoloed()) { anySoloed = true; break; }
    }

    for (auto& bus : m_buses)
        bus->clear(frames);

    m_midiScratch.clear();
    if (m_midiDevice && m_midiDevice->isRunning()) {
        m_midiDevice->markBlockStart(ctx.samplePosition, ctx.sampleRate);
        m_midiDevice->drainInto(m_midiScratch, frames);
    }

    for (auto& track : m_tracks)
        track->renderInput(input, frames);

    if (m_channelRack) {
        m_channelRack->processBlock(ctx,
            [this](TrackId id) -> Track* { return getTrack(id); },
            m_midiScratch);
    }

    for (auto& track : m_tracks) {
        bool shouldPlay = true;
        if (track->isMuted()) shouldPlay = false;
        if (anySoloed && !track->isSoloed()) shouldPlay = false;

        if (!shouldPlay) continue;

        track->processInserts(m_midiScratch, ctx);

        AudioBuffer postFader = track->getPostFaderBuffer(frames);

        BusId destId = track->getOutputBusId();
        Bus* destBus = getBus(destId);
        if (!destBus) destBus = getMasterBus();
        if (destBus) destBus->sumFrom(postFader, frames);
    }

    for (auto& bus : m_buses) {
        if (bus->getId() == kMasterBusId) continue;
        bus->processInserts(m_midiScratch, ctx);
        bus->applyFader(frames);
        Bus* master = getMasterBus();
        if (master) master->sumFrom(bus->getBuffer(frames), frames);
    }

    Bus* master = getMasterBus();
    if (master) {
        master->processInserts(m_midiScratch, ctx);
        master->applyFader(frames);

        AudioBuffer masterBuf = master->getBuffer(frames);

        if (masterBuf.channels > 0 && masterBuf.data) {
            if (masterBuf.channels >= 2)
                m_outputMeter.processSplit(masterBuf.channel(0), masterBuf.channel(1), frames);
            else
                m_outputMeter.processSamples(masterBuf.channel(0), frames);
            m_waveform.write(masterBuf.channel(0), frames);
        }

        int outCh = std::min(output.channels, masterBuf.channels);
        for (int c = 0; c < outCh; ++c)
            std::memcpy(output.channel(c), masterBuf.channel(c), frames * sizeof(float));
    }
}


TrackId AudioEngine::createTrack(const std::string& name, int channels,
                                  InputSource input, TrackId outputBus)
{
    TrackId id = m_nextId.fetch_add(1);
    int block = getBlockSize();

    auto track = std::make_unique<Track>(id, name, channels, block);
    track->setInput(std::move(input));
    track->setOutputBusId(outputBus);

    PC prepCtx;
    prepCtx.sampleRate   = static_cast<double>(getSampleRate());
    prepCtx.maxBlockSize = block;
    track->prepare(prepCtx);

    m_tracks.push_back(std::move(track));

    LOG_INFO("[AudioEngine] Created track '%s' (id=%llu)", name.c_str(), id);
    return id;
}

bool AudioEngine::removeTrack(TrackId id) {
    auto it = std::find_if(m_tracks.begin(), m_tracks.end(),
        [id](const auto& t) { return t->getId() == id; });
    if (it == m_tracks.end()) return false;
    m_tracks.erase(it);
    return true;
}

Track* AudioEngine::getTrack(TrackId id) {
    for (auto& t : m_tracks)
        if (t->getId() == id) return t.get();
    return nullptr;
}

const Track* AudioEngine::getTrack(TrackId id) const {
    for (auto& t : m_tracks)
        if (t->getId() == id) return t.get();
    return nullptr;
}

std::vector<TrackId> AudioEngine::getTrackIds() const {
    std::vector<TrackId> ids;
    ids.reserve(m_tracks.size());
    for (auto& t : m_tracks) ids.push_back(t->getId());
    return ids;
}

int AudioEngine::getTrackCount() const {
    return static_cast<int>(m_tracks.size());
}

BusId AudioEngine::createBus(const std::string& name, int channels) {
    BusId id = m_nextId.fetch_add(1);
    int block = getBlockSize();

    auto bus = std::make_unique<Bus>(id, name, channels, block);
    PC prepCtx;
    prepCtx.sampleRate   = static_cast<double>(getSampleRate());
    prepCtx.maxBlockSize = block;
    bus->prepare(prepCtx);

    m_buses.push_back(std::move(bus));
    return id;
}

bool AudioEngine::removeBus(BusId id) {
    if (id == kMasterBusId) return false;
    auto it = std::find_if(m_buses.begin(), m_buses.end(),
        [id](const auto& b) { return b->getId() == id; });
    if (it == m_buses.end()) return false;
    m_buses.erase(it);
    return true;
}

Bus* AudioEngine::getBus(BusId id) {
    for (auto& b : m_buses)
        if (b->getId() == id) return b.get();
    return nullptr;
}

const Bus* AudioEngine::getBus(BusId id) const {
    for (auto& b : m_buses)
        if (b->getId() == id) return b.get();
    return nullptr;
}

Bus* AudioEngine::getMasterBus() {
    return getBus(kMasterBusId);
}

AudioChain* AudioEngine::getTargetChain(TrackId trackId) {
    if (trackId != kInvalidTrackId) {
        Track* t = getTrack(trackId);
        if (t) return &t->getInsertChain();
    }
    if (!m_tracks.empty())
        return &m_tracks.front()->getInsertChain();
    Bus* master = getMasterBus();
    if (master)
        return &master->getInsertChain();
    return nullptr;
}

AudioChain* AudioEngine::getChain() {
    return getTargetChain(kInvalidTrackId);
}

EffectInfo AudioEngine::addGainPanEffect(TrackId trackId) {
    AudioChain* chain = getTargetChain(trackId);
    if (!chain) return { 0, nullptr, {} };

    auto fx = std::make_unique<GainPan>();
    PC prepCtx;
    prepCtx.sampleRate   = getSampleRate();
    prepCtx.maxBlockSize = getBlockSize();
    fx->prepare(prepCtx);

    auto handles = fx->getParams();
    const char* n = fx->name();
    uint64_t id = m_nextNodeId.fetch_add(1);

    auto proc = BuiltInProcessor::create(id, std::move(fx));
    chain->addNode(proc);
    m_paramMap[id] = std::move(handles);

    return { id, n, m_paramMap[id] };
}

EffectInfo AudioEngine::addDistortionEffect(TrackId trackId) {
    AudioChain* chain = getTargetChain(trackId);
    if (!chain) return { 0, nullptr, {} };

    auto fx = std::make_unique<Distortion>();
    PC prepCtx;
    prepCtx.sampleRate   = getSampleRate();
    prepCtx.maxBlockSize = getBlockSize();
    fx->prepare(prepCtx);

    auto handles = fx->getParams();
    const char* n = fx->name();
    uint64_t id = m_nextNodeId.fetch_add(1);

    auto proc = BuiltInProcessor::create(id, std::move(fx));
    chain->addNode(proc);
    m_paramMap[id] = std::move(handles);

    return { id, n, m_paramMap[id] };
}

EffectInfo AudioEngine::addGainPanEffect()    { return addGainPanEffect(kInvalidTrackId); }
EffectInfo AudioEngine::addDistortionEffect() { return addDistortionEffect(kInvalidTrackId); }

bool AudioEngine::loadVST3PluginSmart(const std::wstring& path, TrackId targetTrack) {
    if (!m_vst3Host) return false;

    auto probe = m_vst3Host->LoadPluginDetached(path);
    if (!probe) return false;

    if (probe->IsInstrument()) {
        std::string name;
        auto wname = probe->GetName();
        name.reserve(wname.size());
        for (wchar_t wc : wname)
            name.push_back(wc < 0x80 ? static_cast<char>(wc) : '?');

        auto gen = std::make_unique<VST3Generator>(std::move(probe), m_vst3Host.get());

        TrackId outTrack = targetTrack;
        if (outTrack == kInvalidTrackId && !m_tracks.empty())
            outTrack = m_tracks.front()->getId();

        return addGeneratorChannel(name, std::move(gen), outTrack) != kInvalidChannelId;
    } else {
        probe.reset();
        return loadVST3Effect(path, targetTrack);
    }
}

ChannelId AudioEngine::loadVST3Instrument(const std::wstring& path,
                                           const std::string& channelName,
                                           TrackId outputTrack) {
    if (!m_vst3Host || !m_channelRack) return kInvalidChannelId;

    auto plugin = m_vst3Host->LoadPluginDetached(path);
    if (!plugin) return kInvalidChannelId;

    std::string name = channelName;
    if (name.empty()) {
        auto wname = plugin->GetName();
        name.reserve(wname.size());
        for (wchar_t wc : wname)
            name.push_back(wc < 0x80 ? static_cast<char>(wc) : '?');
    }

    auto gen = std::make_unique<VST3Generator>(std::move(plugin), m_vst3Host.get());

    TrackId outTrack = outputTrack;
    if (outTrack == kInvalidTrackId && !m_tracks.empty())
        outTrack = m_tracks.front()->getId();

    return m_channelRack->addChannel(name, std::move(gen), outTrack);
}

bool AudioEngine::loadVST3Effect(const std::wstring& path, TrackId trackId) {
    if (!m_vst3Host) return false;

    if (!m_vst3Host->LoadPlugin(path)) return false;

    int pluginIndex = m_vst3Host->GetPluginCount() - 1;
    auto* plugin = m_vst3Host->GetPlugin(pluginIndex);
    if (!plugin) return false;

    if (plugin->IsInstrument()) {
        m_vst3Host->UnloadPlugin(pluginIndex);
        return false;
    }

    uint64_t nodeId = m_nextNodeId.fetch_add(1, std::memory_order_relaxed);

    auto wname = plugin->GetName();
    std::string name;
    name.reserve(wname.size());
    for (wchar_t wc : wname)
        name.push_back(wc < 0x80 ? static_cast<char>(wc) : '?');

    auto proc = VST3Processor::create(nodeId, m_vst3Host.get(), pluginIndex, name);

    AudioChain* chain = getTargetChain(trackId);
    if (!chain) return false;

    chain->addNode(std::move(proc));
    return true;
}

AudioProcessor* AudioEngine::findNodeGlobal(uint64_t nodeId) const {
    for (auto& t : m_tracks) {
        auto* node = t->getInsertChain().findNode(nodeId);
        if (node) return node;
    }
    for (auto& b : m_buses) {
        auto* node = b->getInsertChain().findNode(nodeId);
        if (node) return node;
    }
    return nullptr;
}

void AudioEngine::setNodeBypass(uint64_t nodeId, bool bp) {
    auto* node = findNodeGlobal(nodeId);
    if (node) node->bypass.store(bp, std::memory_order_relaxed);
}

bool AudioEngine::getNodeBypass(uint64_t nodeId) const {
    auto* node = findNodeGlobal(nodeId);
    return node ? node->bypass.load(std::memory_order_relaxed) : false;
}

bool AudioEngine::loadVST3Plugin(const std::wstring& path, TrackId trackId) {
    if (!m_vst3Host) return false;
    if (!m_vst3Host->LoadPlugin(path)) return false;

    AudioChain* chain = getTargetChain(trackId);
    if (!chain) return false;

    int idx = m_vst3Host->GetPluginCount() - 1;
    uint64_t id = m_nextNodeId.fetch_add(1);

    auto name = m_vst3Host->GetPluginNames().back();
    std::string nameStr(name.begin(), name.end());
    auto proc = VST3Processor::create(id, m_vst3Host.get(), idx, nameStr);
    chain->addNode(proc);
    return true;
}

bool AudioEngine::openPluginEditor(int index) {
    return m_vst3Host ? m_vst3Host->OpenPluginEditor(index, nullptr) : false;
}

void AudioEngine::closePluginEditor(int index) {
    if (m_vst3Host) m_vst3Host->ClosePluginEditor(index);
}

float AudioEngine::estimatePitchHz() {
    int N = std::min(4096, getWaveformBufferSize());
    std::vector<float> x;
    m_waveform.readRecent(N, x);
    if (x.empty()) return -1.0f;

    double mean = 0.0;
    for (float s : x) mean += s;
    mean /= static_cast<double>(x.size());
    float maxAbs = 0.0f;

    for (int i = 0; i < static_cast<int>(x.size()); ++i) {
        float s = x[i] - static_cast<float>(mean);
        float w = 0.5f * (1.0f - static_cast<float>(
            std::cos(2.0 * M_PI * static_cast<double>(i) / static_cast<double>(x.size() - 1))));
        s *= w;
        x[i] = s;
        float a = std::fabs(s);
        if (a > maxAbs) maxAbs = a;
    }

    if (maxAbs < 1e-4f) return -1.0f;

    const float clip = 0.6f * maxAbs;
    for (float& s : x) {
        if (s > clip)       s -= clip;
        else if (s < -clip) s += clip;
        else                s = 0.0f;
    }

    int sr = getSampleRate();
    int minLag = static_cast<int>(std::floor(static_cast<double>(sr) / MAX_PITCH_HZ));
    int maxLag = static_cast<int>(std::ceil(static_cast<double>(sr) / MIN_PITCH_HZ));
    minLag = std::max(2, std::min(minLag, static_cast<int>(x.size()) - 1));
    maxLag = std::max(minLag + 2, std::min(maxLag, static_cast<int>(x.size()) - 1));

    std::vector<float> acf(maxLag + 1, 0.0f);
    for (int lag = minLag; lag <= maxLag; ++lag) {
        double sum = 0.0;
        int count = static_cast<int>(x.size()) - lag;
        for (int i = 0; i < count; ++i)
            sum += static_cast<double>(x[i]) * static_cast<double>(x[i + lag]);
        acf[lag] = static_cast<float>(sum);
    }

    double energy = 0.0;
    for (float s : x) energy += static_cast<double>(s) * static_cast<double>(s);
    if (energy <= 1e-12) return -1.0f;
    float norm = static_cast<float>(energy);

    int bestLag = -1;
    float bestVal = -1e30f;
    for (int lag = minLag; lag <= maxLag; ++lag) {
        float val = acf[lag] / norm;
        if (val > bestVal) { bestVal = val; bestLag = lag; }
    }
    if (bestLag <= 0) return -1.0f;

    float refined = parabolicInterp(acf, bestLag);
    if (refined <= 0.0f) refined = static_cast<float>(bestLag);

    float freq = static_cast<float>(sr) / refined;
    return (freq >= MIN_PITCH_HZ && freq <= MAX_PITCH_HZ) ? freq : -1.0f;
}

bool AudioEngine::probeVST3Plugin(const std::wstring& path,
                                   std::string& outName,
                                   bool& outIsInstrument)
{
    if (!m_vst3Host) return false;

    auto probe = m_vst3Host->LoadPluginDetached(path);
    if (!probe) return false;

    auto wname = probe->GetName();
    outName.clear();
    outName.reserve(wname.size());
    for (wchar_t wc : wname)
        outName.push_back(wc < 0x80 ? static_cast<char>(wc) : '?');

    outIsInstrument = probe->IsInstrument();

    probe.reset();
    return true;
}