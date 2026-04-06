#include "audio/Channel.hpp"

#include <algorithm>
#include <cmath>

using namespace gs::audio;

Channel::Channel(ChannelId id, const std::string& name, std::unique_ptr<Generator> gen, int maxBlockSize, int channels)
    : m_id(id)
    , m_name(name)
    , m_channels(channels)
    , m_maxBlockSize(maxBlockSize)
    , m_generator(std::move(gen)) {
    m_audioStorage.resize(channels * maxBlockSize, 0.0f);
    m_audioPtrs.resize(channels);
    for (int c = 0; c < channels; ++c) {
        m_audioPtrs[c] = m_audioStorage.data() + c * maxBlockSize;
    }

    m_midiStorage.resize(512);
    m_midi.events = m_midiStorage.data();
    m_midi.count = 0;
    m_midi.capacity = static_cast<int>(m_midiStorage.size());
}

ChannelId Channel::getId() const {
    return m_id;
}

const std::string& Channel::getName() const {
    return m_name;
}

void Channel::setName(const std::string& n) {
    m_name = n;
}

Generator* Channel::getGenerator() {
    return m_generator.get();
}

const Generator* Channel::getGenerator() const {
    return m_generator.get();
}

GeneratorBase* Channel::getGeneratorBase() {
    return dynamic_cast<GeneratorBase*>(m_generator.get());
}

TrackId Channel::getOutputTrackId() const {
    return m_outputTrackId;
}

void Channel::setOutputTrackId(TrackId id) {
    m_outputTrackId = id;
}

float Channel::getVolume() const {
    return m_volume.load(std::memory_order_relaxed);
}

void Channel::setVolume(float v) {
    m_volume.store(std::clamp(v, 0.0f, 2.0f), std::memory_order_relaxed);
}

float Channel::getPan() const {
    return m_pan.load(std::memory_order_relaxed);
}

void Channel::setPan(float p) {
    m_pan.store(std::clamp(p, -1.0f, 1.0f), std::memory_order_relaxed);
}

bool Channel::isMuted() const {
    return m_mute.load(std::memory_order_relaxed);
}

void Channel::setMute(bool m) {
    m_mute.store(m, std::memory_order_relaxed);
}

bool Channel::isArmed() const {
    return m_armed.load(std::memory_order_relaxed);
}

void Channel::setArmed(bool a) {
    m_armed.store(a, std::memory_order_relaxed);
}

MidiBuffer& Channel::getMidiBuffer() {
    return m_midi;
}

void Channel::clearMidi() {
    m_midi.clear();
}

void Channel::prepare(const PC& ctx) {
    if (m_generator) {
        m_generator->prepare(ctx);
    }

    if (ctx.maxBlockSize > m_maxBlockSize) {
        m_maxBlockSize = ctx.maxBlockSize;
        m_audioStorage.resize(m_channels * m_maxBlockSize, 0.0f);
        for (int c = 0; c < m_channels; ++c) {
            m_audioPtrs[c] = m_audioStorage.data() + c * m_maxBlockSize;
        }
    }
}

AudioBuffer Channel::process(const PC& ctx) {
    int frames = ctx.blockSize;
    AudioBuffer buf{m_audioPtrs.data(), m_channels, frames};

    buf.clear();

    if (m_mute.load(std::memory_order_relaxed) || !m_generator) {
        return buf;
    }

    m_generator->generate(buf, m_midi, ctx);

    float vol = m_volume.load(std::memory_order_relaxed);
    float pan = m_pan.load(std::memory_order_relaxed);

    if (m_channels >= 2) {
        float angle = (pan * 0.5f + 0.5f) * static_cast<float>(M_PI) * 0.5f;
        float lGain = std::cos(angle) * vol;
        float rGain = std::sin(angle) * vol;

        float* L = buf.channel(0);
        float* R = buf.channel(1);
        for (int i = 0; i < frames; ++i) {
            L[i] *= lGain;
            R[i] *= rGain;
        }
    } else {
        buf.applyGain(vol);
    }

    return buf;
}

int Channel::getChannels() const {
    return m_channels;
}