#include "audio/ChannelRack.hpp"

#include <algorithm>
#include <cmath>

ChannelId ChannelRack::addChannel(const std::string& name, std::unique_ptr<Generator> gen, TrackId outputTrack) {
    ChannelId id = m_nextChannelId++;
    auto ch = std::make_unique<Channel>(id, name, std::move(gen), m_maxBlockSize, 2);
    ch->setOutputTrackId(outputTrack);

    if (m_prepared) {
        PC ctx;
        ctx.sampleRate = m_sampleRate;
        ctx.maxBlockSize = m_maxBlockSize;
        ch->prepare(ctx);
    }

    m_channels.push_back(std::move(ch));

    if (m_focusedChannelId.load(std::memory_order_relaxed) == kInvalidChannelId) {
        m_focusedChannelId.store(id, std::memory_order_relaxed);
    }

    return id;
}

bool ChannelRack::removeChannel(ChannelId id) {
    auto it = std::find_if(m_channels.begin(), m_channels.end(), [id](const auto& c) {
        return c->getId() == id;
    });
    if (it == m_channels.end()) {
        return false;
    }

    m_channelPatterns.erase(id);
    m_channels.erase(it);

    if (m_focusedChannelId.load(std::memory_order_relaxed) == id) {
        if (!m_channels.empty()) {
            m_focusedChannelId.store(m_channels.front()->getId(), std::memory_order_relaxed);
        } else {
            m_focusedChannelId.store(kInvalidChannelId, std::memory_order_relaxed);
        }
    }

    return true;
}

Channel* ChannelRack::getChannel(ChannelId id) {
    for (auto& c : m_channels) {
        if (c->getId() == id) {
            return c.get();
        }
    }
    return nullptr;
}

const Channel* ChannelRack::getChannel(ChannelId id) const {
    for (auto& c : m_channels) {
        if (c->getId() == id) {
            return c.get();
        }
    }
    return nullptr;
}

std::vector<ChannelId> ChannelRack::getChannelIds() const {
    std::vector<ChannelId> ids;
    ids.reserve(m_channels.size());
    for (auto& c : m_channels) {
        ids.push_back(c->getId());
    }
    return ids;
}

int ChannelRack::getChannelCount() const {
    return static_cast<int>(m_channels.size());
}

ChannelId ChannelRack::getFocusedChannelId() const {
    return m_focusedChannelId.load(std::memory_order_relaxed);
}

void ChannelRack::setFocusedChannel(ChannelId id) {
    m_focusedChannelId.store(id, std::memory_order_relaxed);
}

bool ChannelRack::isFocused(ChannelId id) const {
    return m_focusedChannelId.load(std::memory_order_relaxed) == id;
}

PatternId ChannelRack::createPattern(const std::string& name, int lengthBars) {
    PatternId id = m_nextPatternId++;
    int ticks = lengthBars * kDefaultPPQN * 4;
    m_patterns[id] = std::make_unique<Pattern>(id, name, ticks);
    return id;
}

bool ChannelRack::removePattern(PatternId id) {
    for (auto& [chId, patId] : m_channelPatterns) {
        if (patId == id) {
            patId = kInvalidPatternId;
        }
    }
    return m_patterns.erase(id) > 0;
}

Pattern* ChannelRack::getPattern(PatternId id) {
    auto it = m_patterns.find(id);
    return it != m_patterns.end() ? it->second.get() : nullptr;
}

const Pattern* ChannelRack::getPattern(PatternId id) const {
    auto it = m_patterns.find(id);
    return it != m_patterns.end() ? it->second.get() : nullptr;
}

std::vector<PatternId> ChannelRack::getPatternIds() const {
    std::vector<PatternId> ids;
    ids.reserve(m_patterns.size());
    for (auto& [id, _] : m_patterns) {
        ids.push_back(id);
    }
    return ids;
}

void ChannelRack::setChannelPattern(ChannelId channelId, PatternId patternId) {
    m_channelPatterns[channelId] = patternId;
}

PatternId ChannelRack::getChannelPattern(ChannelId channelId) const {
    auto it = m_channelPatterns.find(channelId);
    return it != m_channelPatterns.end() ? it->second : kInvalidPatternId;
}

void ChannelRack::play() {
    m_playing.store(true, std::memory_order_release);
}

void ChannelRack::stop() {
    m_playing.store(false, std::memory_order_release);
    m_tickPosition.store(0.0, std::memory_order_release);
    for (auto& ch : m_channels) {
        if (ch->getGenerator()) {
            ch->getGenerator()->reset();
        }
    }
}

void ChannelRack::pause() {
    m_playing.store(false, std::memory_order_release);
}

bool ChannelRack::isPlaying() const {
    return m_playing.load(std::memory_order_acquire);
}

double ChannelRack::getTickPosition() const {
    return m_tickPosition.load(std::memory_order_relaxed);
}

void ChannelRack::setTickPosition(double t) {
    m_tickPosition.store(t, std::memory_order_relaxed);
}

double ChannelRack::getBpm() const {
    return m_bpm.load(std::memory_order_relaxed);
}

void ChannelRack::setBpm(double bpm) {
    m_bpm.store(std::max(20.0, std::min(bpm, 999.0)), std::memory_order_relaxed);
}

bool ChannelRack::isLooping() const {
    return m_loop.load(std::memory_order_relaxed);
}

void ChannelRack::setLooping(bool l) {
    m_loop.store(l, std::memory_order_relaxed);
}

void ChannelRack::prepare(double sampleRate, int maxBlockSize) {
    m_sampleRate = sampleRate;
    m_maxBlockSize = maxBlockSize;
    m_prepared = true;

    PC ctx;
    ctx.sampleRate = sampleRate;
    ctx.maxBlockSize = maxBlockSize;

    for (auto& ch : m_channels) {
        ch->prepare(ctx);
    }
}

void ChannelRack::processBlock(const PC& ctx, const std::function<Track*(TrackId)>& getTrack, const MidiBuffer& liveMidi) {
    bool playing = m_playing.load(std::memory_order_acquire);
    double bpm = m_bpm.load(std::memory_order_relaxed);
    double tickPos = m_tickPosition.load(std::memory_order_relaxed);
    double sampleRate = ctx.sampleRate;
    int frames = ctx.blockSize;

    double ticksPerSample = (bpm * kDefaultPPQN) / (60.0 * sampleRate);
    double ticksThisBlock = ticksPerSample * frames;
    double tickEnd = tickPos + ticksThisBlock;

    PC genCtx = ctx;
    genCtx.bpm = bpm;
    genCtx.isPlaying = playing;

    ChannelId focusedId = m_focusedChannelId.load(std::memory_order_relaxed);

    for (auto& ch : m_channels) {
        ChannelId chId = ch->getId();

        ch->clearMidi();

        if (playing) {
            PatternId patId = getChannelPattern(chId);
            if (patId != kInvalidPatternId) {
                Pattern* pat = getPattern(patId);
                if (pat && !pat->isEmpty()) {
                    pat->sortAll();
                    emitPatternToMidiForChannel(*pat, chId, ch->getMidiBuffer(), tickPos, tickEnd, bpm, sampleRate, frames);
                }
            }
        }

        bool receivesMidi = (chId == focusedId) || ch->isArmed();
        if (receivesMidi && liveMidi.count > 0) {
            for (int i = 0; i < liveMidi.count; ++i) {
                ch->getMidiBuffer().add(liveMidi.events[i]);
            }
        }

        AudioBuffer channelOut = ch->process(genCtx);

        TrackId destId = ch->getOutputTrackId();
        Track* destTrack = getTrack(destId);
        if (destTrack) {
            AudioBuffer trackBuf = destTrack->getBuffer(frames);
            trackBuf.mixFrom(channelOut, 1.0f);
        }
    }

    if (playing) {
        double newPos = tickEnd;

        if (m_loop.load(std::memory_order_relaxed)) {
            int maxLen = 0;
            for (auto& ch : m_channels) {
                PatternId patId = getChannelPattern(ch->getId());
                if (patId != kInvalidPatternId) {
                    Pattern* pat = getPattern(patId);
                    if (pat) {
                        maxLen = std::max(maxLen, pat->getLengthTicks());
                    }
                }
            }
            if (maxLen > 0 && newPos >= maxLen) {
                newPos = std::fmod(newPos, static_cast<double>(maxLen));
            }
        }

        m_tickPosition.store(newPos, std::memory_order_relaxed);
    }
}

void ChannelRack::sendMidiToChannel(ChannelId channelId, const MidiEvent& event) {
    Channel* ch = getChannel(channelId);
    if (ch) {
        ch->getMidiBuffer().add(event);
    }
}

EffectInfo ChannelRack::getChannelGeneratorInfo(ChannelId channelId) {
    Channel* ch = getChannel(channelId);
    if (!ch) {
        return {0, nullptr, {}};
    }

    auto* genBase = ch->getGeneratorBase();
    if (!genBase) {
        return {channelId, ch->getGenerator()->name(), {}};
    }

    return {channelId, genBase->name(), genBase->getParams()};
}

void ChannelRack::emitPatternToMidiForChannel(
    const Pattern& pat,
    ChannelId channelId,
    MidiBuffer& out,
    double tickStart,
    double tickEnd,
    double bpm,
    double sampleRate,
    int blockFrames
) {
    if (pat.isEmpty() || pat.getLengthTicks() <= 0 || blockFrames <= 0 || sampleRate <= 0.0) {
        return;
    }

    const double ticksPerSample = (bpm * pat.getPpqn()) / (60.0 * sampleRate);
    if (ticksPerSample <= 0.0) {
        return;
    }

    const double samplesPerTick = 1.0 / ticksPerSample;
    double cursor = tickStart;

    while (cursor < tickEnd) {
        double patternPos = std::fmod(cursor, static_cast<double>(pat.getLengthTicks()));
        if (patternPos < 0.0) {
            patternPos += static_cast<double>(pat.getLengthTicks());
        }

        const double remaining = tickEnd - cursor;
        const double patternRemaining = static_cast<double>(pat.getLengthTicks()) - patternPos;
        const double span = std::min(remaining, patternRemaining);
        const double spanEnd = patternPos + span;

        for (const auto& note : pat.getNotes()) {
            if (note.muted) {
                continue;
            }

            if (note.targetChannelId != kInvalidChannelId && note.targetChannelId != channelId) {
                continue;
            }

            const double noteTick = static_cast<double>(note.tick);
            const double noteEnd = noteTick + static_cast<double>(note.duration);

            if (noteTick >= patternPos && noteTick < spanEnd) {
                const double sampleExact = (cursor - tickStart + (noteTick - patternPos)) * samplesPerTick;
                const int sampleOffset = std::clamp(static_cast<int>(sampleExact), 0, blockFrames - 1);
                const int velocity = std::clamp(static_cast<int>(note.velocity * 127.0f), 0, 127);
                out.add(MidiEvent::noteOn(0, note.note, velocity, sampleOffset));
            }

            if (noteEnd >= patternPos && noteEnd < spanEnd) {
                const double sampleExact = (cursor - tickStart + (noteEnd - patternPos)) * samplesPerTick;
                const int sampleOffset = std::clamp(static_cast<int>(sampleExact), 0, blockFrames - 1);
                out.add(MidiEvent::noteOff(0, note.note, sampleOffset));
            }
        }

        cursor += span;
    }
}
