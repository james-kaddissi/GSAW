#pragma once

#include "audio/Ids.hpp"

#include <atomic>
#include <cstdint>

enum class PlaybackState : uint8_t {
    Stopped,
    Playing,
    Paused
};

enum class PlaybackMode : uint8_t {
    Song,
    Pattern
};

struct LoopRange {
    bool enabled = false;
    int startTick = 0;
    int endTick = 0;

    bool isValid() const {
        return endTick > startTick;
    }
};

struct TimeSignature {
    int numerator = 4;
    int denominator = 4;
};

struct TransportBlockInfo {
    int startTick = 0;
    int endTick = 0;
    int startSampleOffset = 0;
    int blockFrames = 0;
    double bpm = 120.0;
    double ticksPerSecond = 0.0;
    bool loopWrapped = false;
};

class Transport {
public:
    explicit Transport(int ppqn = 96);

    PlaybackState getPlaybackState() const;
    void setPlaybackState(PlaybackState state);

    bool isPlaying() const;
    bool isPaused() const;
    bool isStopped() const;

    void play();
    void stop();
    void pause();
    void togglePlayPause();

    PlaybackMode getPlaybackMode() const;
    void setPlaybackMode(PlaybackMode mode);

    int getCurrentTick() const;
    void setCurrentTick(int tick);

    void rewindToStart();

    int getPpqn() const;
    void setPpqn(int ppqn);

    double getTempoBpm() const;
    void setTempoBpm(double bpm);

    const TimeSignature& getTimeSignature() const;
    void setTimeSignature(TimeSignature sig);

    const LoopRange& getLoopRange() const;
    void setLoopRange(const LoopRange& range);
    void clearLoopRange();

    bool isLooping() const;
    void setLooping(bool enabled);

    PatternId getSelectedPatternId() const;
    void setSelectedPatternId(PatternId patternId);

    double ticksPerBeat() const;
    double ticksPerBar() const;
    double ticksPerSecond() const;
    double secondsPerTick() const;

    double tickToSeconds(int tick) const;
    int secondsToTick(double seconds) const;

    TransportBlockInfo beginBlock(int blockFrames, double sampleRate);
    void advanceBlock(int blockFrames, double sampleRate);

private:
    static int clampTick(int tick);
    static double clampTempo(double bpm);
    static int clampPpqn(int ppqn);
    static TimeSignature sanitizeTimeSignature(TimeSignature sig);
    static LoopRange sanitizeLoopRange(const LoopRange& range);

private:
    std::atomic<PlaybackState> m_state{PlaybackState::Stopped};
    std::atomic<PlaybackMode> m_mode{PlaybackMode::Song};

    std::atomic<int> m_currentTick{0};
    std::atomic<int> m_ppqn{96};

    std::atomic<double> m_tempoBpm{120.0};
    TimeSignature m_timeSignature{4, 4};
    LoopRange m_loopRange{};

    std::atomic<bool> m_looping{false};


    std::atomic<PatternId> m_selectedPatternId{kInvalidPatternId};
};
