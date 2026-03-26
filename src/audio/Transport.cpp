#include "audio/Transport.hpp"

#include <algorithm>
#include <cmath>

Transport::Transport(int ppqn)
    : m_state(PlaybackState::Stopped),
      m_mode(PlaybackMode::Song),
      m_currentTick(0),
      m_ppqn(clampPpqn(ppqn)),
      m_tempoBpm(120.0),
      m_timeSignature{4, 4},
      m_loopRange{},
      m_looping(false),
      m_selectedPatternId(0) {}

PlaybackState Transport::getPlaybackState() const
{
    return m_state.load(std::memory_order_relaxed);
}

void Transport::setPlaybackState(PlaybackState state)
{
    m_state.store(state, std::memory_order_relaxed);
}

bool Transport::isPlaying() const
{
    return getPlaybackState() == PlaybackState::Playing;
}

bool Transport::isPaused() const
{
    return getPlaybackState() == PlaybackState::Paused;
}

bool Transport::isStopped() const
{
    return getPlaybackState() == PlaybackState::Stopped;
}

void Transport::play()
{
    m_state.store(PlaybackState::Playing, std::memory_order_relaxed);
}

void Transport::stop()
{
    m_state.store(PlaybackState::Stopped, std::memory_order_relaxed);
    m_currentTick.store(0, std::memory_order_relaxed);
}

void Transport::pause()
{
    m_state.store(PlaybackState::Paused, std::memory_order_relaxed);
}

void Transport::togglePlayPause()
{
    const PlaybackState state = getPlaybackState();
    if (state == PlaybackState::Playing)
    {
        pause();
    }
    else
    {
        play();
    }
}

PlaybackMode Transport::getPlaybackMode() const
{
    return m_mode.load(std::memory_order_relaxed);
}

void Transport::setPlaybackMode(PlaybackMode mode)
{
    m_mode.store(mode, std::memory_order_relaxed);
}

int Transport::getCurrentTick() const
{
    return m_currentTick.load(std::memory_order_relaxed);
}

void Transport::setCurrentTick(int tick)
{
    m_currentTick.store(clampTick(tick), std::memory_order_relaxed);
}

void Transport::rewindToStart()
{
    m_currentTick.store(0, std::memory_order_relaxed);
}

int Transport::getPpqn() const
{
    return m_ppqn.load(std::memory_order_relaxed);
}

void Transport::setPpqn(int ppqn)
{
    m_ppqn.store(clampPpqn(ppqn), std::memory_order_relaxed);
}

double Transport::getTempoBpm() const
{
    return m_tempoBpm.load(std::memory_order_relaxed);
}

void Transport::setTempoBpm(double bpm)
{
    m_tempoBpm.store(clampTempo(bpm), std::memory_order_relaxed);
}

const TimeSignature &Transport::getTimeSignature() const
{
    return m_timeSignature;
}

void Transport::setTimeSignature(TimeSignature sig)
{
    m_timeSignature = sanitizeTimeSignature(sig);
}

const LoopRange &Transport::getLoopRange() const
{
    return m_loopRange;
}

void Transport::setLoopRange(const LoopRange &range)
{
    m_loopRange = sanitizeLoopRange(range);
}

void Transport::clearLoopRange()
{
    m_loopRange = {};
}

bool Transport::isLooping() const
{
    return m_looping.load(std::memory_order_relaxed);
}

void Transport::setLooping(bool enabled)
{
    m_looping.store(enabled, std::memory_order_relaxed);
}

PatternId Transport::getSelectedPatternId() const
{
    return m_selectedPatternId.load(std::memory_order_relaxed);
}

void Transport::setSelectedPatternId(PatternId patternId)
{
    m_selectedPatternId.store(std::max(static_cast<PatternId>(0), patternId), std::memory_order_relaxed);
}

double Transport::ticksPerBeat() const
{
    return static_cast<double>(getPpqn());
}

double Transport::ticksPerBar() const
{
    return ticksPerBeat() * static_cast<double>(m_timeSignature.numerator);
}

double Transport::ticksPerSecond() const
{
    return (getTempoBpm() / 60.0) * ticksPerBeat();
}

double Transport::secondsPerTick() const
{
    const double tps = ticksPerSecond();
    return tps > 0.0 ? (1.0 / tps) : 0.0;
}

double Transport::tickToSeconds(int tick) const
{
    return static_cast<double>(std::max(0, tick)) * secondsPerTick();
}

int Transport::secondsToTick(double seconds) const
{
    if (seconds <= 0.0)
    {
        return 0;
    }
    return static_cast<int>(std::llround(seconds * ticksPerSecond()));
}

TransportBlockInfo Transport::beginBlock(int blockFrames, double sampleRate)
{
    TransportBlockInfo info;
    info.blockFrames = std::max(0, blockFrames);
    info.bpm = getTempoBpm();
    info.ticksPerSecond = ticksPerSecond();
    info.startTick = getCurrentTick();
    info.startSampleOffset = 0;
    info.endTick = info.startTick;
    info.loopWrapped = false;

    if (!isPlaying() || info.blockFrames <= 0 || sampleRate <= 0.0)
    {
        return info;
    }

    const double blockSeconds = static_cast<double>(info.blockFrames) / sampleRate;
    const int deltaTicks = std::max(0, static_cast<int>(std::llround(blockSeconds * info.ticksPerSecond)));
    int newTick = info.startTick + deltaTicks;

    if (isLooping() && m_loopRange.enabled && m_loopRange.isValid())
    {
        const int loopStart = m_loopRange.startTick;
        const int loopEnd = m_loopRange.endTick;

        if (info.startTick >= loopEnd)
        {
            info.startTick = loopStart;
            m_currentTick.store(loopStart, std::memory_order_relaxed);
            newTick = loopStart + deltaTicks;
        }

        if (newTick >= loopEnd)
        {
            const int loopLen = loopEnd - loopStart;
            if (loopLen > 0)
            {
                const int overshoot = newTick - loopStart;
                newTick = loopStart + (overshoot % loopLen);
                info.loopWrapped = true;
            }
            else
            {
                newTick = loopStart;
                info.loopWrapped = true;
            }
        }
    }

    info.endTick = newTick;
    return info;
}

void Transport::advanceBlock(int blockFrames, double sampleRate)
{
    if (!isPlaying() || blockFrames <= 0 || sampleRate <= 0.0)
    {
        return;
    }

    const double blockSeconds = static_cast<double>(blockFrames) / sampleRate;
    const int deltaTicks = std::max(0, static_cast<int>(std::llround(blockSeconds * ticksPerSecond())));
    int currentTick = getCurrentTick();
    int newTick = currentTick + deltaTicks;

    if (isLooping() && m_loopRange.enabled && m_loopRange.isValid())
    {
        const int loopStart = m_loopRange.startTick;
        const int loopEnd = m_loopRange.endTick;

        if (currentTick >= loopEnd)
        {
            currentTick = loopStart;
            newTick = loopStart + deltaTicks;
        }

        if (newTick >= loopEnd)
        {
            const int loopLen = loopEnd - loopStart;
            if (loopLen > 0)
            {
                const int overshoot = newTick - loopStart;
                newTick = loopStart + (overshoot % loopLen);
            }
            else
            {
                newTick = loopStart;
            }
        }
    }

    m_currentTick.store(clampTick(newTick), std::memory_order_relaxed);
}

int Transport::clampTick(int tick)
{
    return std::max(0, tick);
}

double Transport::clampTempo(double bpm)
{
    return std::max(1.0, bpm);
}

int Transport::clampPpqn(int ppqn)
{
    return std::max(1, ppqn);
}

TimeSignature Transport::sanitizeTimeSignature(TimeSignature sig)
{
    sig.numerator = std::max(1, sig.numerator);
    sig.denominator = std::max(1, sig.denominator);
    return sig;
}

LoopRange Transport::sanitizeLoopRange(const LoopRange &range)
{
    LoopRange result = range;
    result.startTick = std::max(0, result.startTick);
    result.endTick = std::max(0, result.endTick);

    if (result.endTick <= result.startTick)
    {
        result.enabled = false;
        result.startTick = 0;
        result.endTick = 0;
    }

    return result;
}
