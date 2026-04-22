#include "audio/AudioLayer.hpp"

void AudioLayer::configure(EngineConfig config) {
    m_config = std::move(config);
}

void AudioLayer::onAttach(gs::core::AppContext& ctx) {
    (void)ctx;
    m_engine = std::make_unique<AudioEngine>();
    m_initialized = m_engine->initialize(m_config);
}

void AudioLayer::onDetach(gs::core::AppContext& ctx) {
    (void)ctx;
    if (m_engine) {
        m_engine->shutdown();
        m_engine.reset();
    }
    m_initialized = false;
}

void AudioLayer::onUpdate(gs::core::AppContext& ctx, float dt) {
    (void)dt;
    if (!m_initialized) return;

    float rms = m_engine->getRmsDb();
    float peak = m_engine->getPeakDb();

    constexpr float kThreshold = 0.1f;
    if (std::abs(rms - m_cachedRmsDb) > kThreshold || std::abs(peak - m_cachedPeakDb) > kThreshold) {
        m_cachedRmsDb = rms;
        m_cachedPeakDb = peak;
        ctx.requestRender();
    }
}

void AudioLayer::onRender(gs::core::AppContext& ctx) {
    (void)ctx;
}

bool AudioLayer::isInitialized() const {
    return m_initialized;
}

float AudioLayer::getRmsDb() const {
    return m_initialized ? m_engine->getRmsDb() : -96.0f;
}

float AudioLayer::getPeakDb() const {
    return m_initialized ? m_engine->getPeakDb() : -96.0f;
}

void AudioLayer::getRecentWaveform(int numSamples, std::vector<float>& out) const {
    if (!m_initialized) {
        out.clear();
        return;
    }
    m_engine->getRecentWaveform(numSamples, out);
}

int AudioLayer::getSampleRate() const {
    return m_initialized ? m_engine->getSampleRate() : 48000;
}

int AudioLayer::getWaveformBufferSize() const {
    return m_initialized ? m_engine->getWaveformBufferSize() : 0;
}

float AudioLayer::estimatePitchHz() const {
    return m_initialized ? const_cast<AudioEngine*>(m_engine.get())->estimatePitchHz() : 0.0f;
}

void AudioLayer::toggleBypass() {
    if (m_initialized) m_engine->toggleBypass();
}

void AudioLayer::setBypass(bool enabled) {
    if (m_initialized) m_engine->setBypass(enabled);
}

bool AudioLayer::isBypassed() const {
    return m_initialized && m_engine->isBypassed();
}

void AudioLayer::toggleNodeBypass(uint64_t nodeId) {
    if (!m_initialized) return;
    m_engine->setNodeBypass(nodeId, !m_engine->getNodeBypass(nodeId));
}

bool AudioLayer::isNodeBypassed(uint64_t nodeId) const {
    return m_initialized ? m_engine->getNodeBypass(nodeId) : false;
}

TrackId AudioLayer::createTrack(const std::string& name, int channels, InputSource input, TrackId outputBus) {
    if (!m_initialized) return kInvalidTrackId;
    return m_engine->createTrack(name, channels, std::move(input), outputBus);
}

bool AudioLayer::removeTrack(TrackId id) {
    return m_initialized ? m_engine->removeTrack(id) : false;
}

Track* AudioLayer::getTrack(TrackId id) {
    return m_initialized ? m_engine->getTrack(id) : nullptr;
}

std::vector<TrackId> AudioLayer::getTrackIds() const {
    return m_initialized ? m_engine->getTrackIds() : std::vector<TrackId>{};
}

int AudioLayer::getTrackCount() const {
    return m_initialized ? m_engine->getTrackCount() : 0;
}

BusId AudioLayer::createBus(const std::string& name, int channels) {
    if (!m_initialized) return 0;
    return m_engine->createBus(name, channels);
}

Bus* AudioLayer::getBus(BusId id) {
    return m_initialized ? m_engine->getBus(id) : nullptr;
}

Bus* AudioLayer::getMasterBus() {
    return m_initialized ? m_engine->getMasterBus() : nullptr;
}

std::shared_ptr<EffectStrip> AudioLayer::createEffectStrip(EffectBinding binding, EffectStripConfig cfg) {
    auto bypassFn = [this](uint64_t id) { toggleNodeBypass(id); };
    return EffectStrip::create(std::move(binding), std::move(bypassFn), std::move(cfg));
}

std::optional<std::wstring> AudioLayer::pickPluginPathVST3(PlatformWindowHandle owner) {
#ifdef _WIN32
    OPENFILENAMEW ofn{};
    wchar_t fileName[MAX_PATH] = L"";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(owner);
    ofn.lpstrFilter = L"VST3 Plugin (*.vst3)\0*.vst3\0All Files (*.*)\0*.*\0\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = L"vst3";
    if (!GetOpenFileNameW(&ofn)) return std::nullopt;
    return std::wstring(fileName);
#else
    (void)owner;
    return std::nullopt;
#endif
}

bool AudioLayer::loadVST3Plugin(const std::wstring& path) {
    return m_initialized ? m_engine->loadVST3Plugin(path) : false;
}

bool AudioLayer::openPluginEditor(int index) {
    return m_initialized ? m_engine->openPluginEditor(index) : false;
}

void AudioLayer::closePluginEditor(int index) {
    if (m_initialized) m_engine->closePluginEditor(index);
}

EffectBinding AudioLayer::addGainPanEffect(TrackId trackId) {
    if (!m_initialized) return EffectBinding{};
    return EffectBinding{ m_engine->addGainPanEffect(trackId) };
}

EffectBinding AudioLayer::addDistortionEffect(TrackId trackId) {
    if (!m_initialized) return EffectBinding{};
    return EffectBinding{ m_engine->addDistortionEffect(trackId) };
}

VST3Host* AudioLayer::getVST3Host() {
    return m_initialized ? m_engine->getVST3Host() : nullptr;
}

AudioEngine* AudioLayer::getEngine() {
    return m_initialized ? m_engine.get() : nullptr;
}

ChannelRack* AudioLayer::getChannelRack() {
    return m_initialized ? m_engine->getChannelRack() : nullptr;
}

bool AudioLayer::probeVST3Plugin(const std::wstring& path, std::string& outName, bool& outIsInstrument) {
    return m_initialized ? m_engine->probeVST3Plugin(path, outName, outIsInstrument) : false;
}