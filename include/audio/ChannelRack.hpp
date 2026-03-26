#pragma once

#include "audio/Channel.hpp"
#include "audio/Pattern.hpp"
#include "audio/Ids.hpp"

#include <audio/core/AudioTypes.hpp>
#include <audio/core/Track.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using PC = gs::audio::ProcessContext;
using namespace gs::audio;

class ChannelRack {
public:
    ChannelRack() = default;

    ChannelId addChannel(const std::string& name, std::unique_ptr<Generator> gen, TrackId outputTrack = kInvalidTrackId);
    bool removeChannel(ChannelId id);

    Channel* getChannel(ChannelId id);
    const Channel* getChannel(ChannelId id) const;

    std::vector<ChannelId> getChannelIds() const;
    int getChannelCount() const;

    ChannelId getFocusedChannelId() const;
    void setFocusedChannel(ChannelId id);
    bool isFocused(ChannelId id) const;

    PatternId createPattern(const std::string& name, int lengthBars = 4);
    bool removePattern(PatternId id);

    Pattern* getPattern(PatternId id);
    const Pattern* getPattern(PatternId id) const;

    std::vector<PatternId> getPatternIds() const;

    void setChannelPattern(ChannelId channelId, PatternId patternId);
    PatternId getChannelPattern(ChannelId channelId) const;

    void play();
    void stop();
    void pause();

    bool isPlaying() const;

    double getTickPosition() const;
    void setTickPosition(double t);

    double getBpm() const;
    void setBpm(double bpm);

    bool isLooping() const;
    void setLooping(bool l);

    void prepare(double sampleRate, int maxBlockSize);

    void processBlock(const PC& ctx, const std::function<Track*(TrackId)>& getTrack, const MidiBuffer& liveMidi);

    void sendMidiToChannel(ChannelId channelId, const MidiEvent& event);
    EffectInfo getChannelGeneratorInfo(ChannelId channelId);

private:
    std::vector<std::unique_ptr<Channel>> m_channels;
    std::unordered_map<PatternId, std::unique_ptr<Pattern>> m_patterns;
    std::unordered_map<ChannelId, PatternId> m_channelPatterns;
    std::atomic<ChannelId> m_focusedChannelId{kInvalidChannelId};
    std::atomic<bool> m_playing{false};
    std::atomic<double> m_tickPosition{0.0};
    std::atomic<double> m_bpm{120.0};
    std::atomic<bool> m_loop{true};
    uint64_t m_nextChannelId = 1;
    uint64_t m_nextPatternId = 1;
    double m_sampleRate = 48000.0;
    int m_maxBlockSize = 512;
    bool m_prepared = false;
};