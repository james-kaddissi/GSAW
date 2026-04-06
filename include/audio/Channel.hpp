#pragma once

#include "audio/Ids.hpp"

#include <audio/core/AudioTypes.hpp>
#include <audio/core/Track.hpp>
#include <audio/dsp/generators/Generator.hpp>
#include <audio/dsp/generators/BaseGenerator.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.141592653589793238462643383279502884
#endif

using PC = gs::audio::ProcessContext;

class Channel {
public:
    Channel(ChannelId id, const std::string& name, std::unique_ptr<gs::audio::Generator> gen, int maxBlockSize, int channels = 2);

    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;
    Channel(Channel&&) noexcept = default;
    Channel& operator=(Channel&&) noexcept = default;

    ChannelId getId() const;
    const std::string& getName() const;
    void setName(const std::string& n);

    gs::audio::Generator* getGenerator();
    const gs::audio::Generator* getGenerator() const;
    gs::audio::GeneratorBase* getGeneratorBase();

    TrackId getOutputTrackId() const;
    void setOutputTrackId(TrackId id);

    float getVolume() const;
    void setVolume(float v);

    float getPan() const;
    void setPan(float p);

    bool isMuted() const;
    void setMute(bool m);

    bool isArmed() const;
    void setArmed(bool a);

    gs::audio::MidiBuffer& getMidiBuffer();
    void clearMidi();

    void prepare(const PC& ctx);
    gs::audio::AudioBuffer process(const PC& ctx);

    int getChannels() const;

private:
    ChannelId m_id;
    std::string m_name;
    int m_channels;
    int m_maxBlockSize;
    std::unique_ptr<gs::audio::Generator> m_generator;
    TrackId m_outputTrackId = kInvalidTrackId;
    std::atomic<float> m_volume{1.0f};
    std::atomic<float> m_pan{0.0f};
    std::atomic<bool> m_mute{false};
    std::atomic<bool> m_armed{false};
    std::vector<float> m_audioStorage;
    std::vector<float*> m_audioPtrs;
    std::vector<gs::audio::MidiEvent> m_midiStorage;
    gs::audio::MidiBuffer m_midi;
};