#pragma once

#include <audio/core/AudioConfig.hpp>
#include <audio/core/AudioTypes.hpp>
#include <audio/core/Track.hpp>
#include <audio/core/Bus.hpp>
#include <audio/device/AudioDeviceIO.hpp>
#include <audio/device/MidiDeviceIO.hpp>
#include <audio/dsp/AudioChain.hpp>
#include <audio/dsp/Metering.hpp>
#include <audio/plugins/VST3Host.hpp>

#include "audio/ChannelRack.hpp"

#include <atomic>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

using PC = gs::audio::ProcessContext;
using namespace gs::audio;

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    bool initialize(const EngineConfig& config);
    void shutdown();
    bool isRunning() const;

    int getSampleRate() const;
    int getBlockSize() const;
    int getInputChannels() const;
    int getOutputChannels() const;
    std::string getDeviceName() const;

    void toggleBypass();
    void setBypass(bool enabled);
    bool isBypassed() const;

    float getRmsDb() const;
    float getPeakDb() const;
    void getRecentWaveform(int numSamples, std::vector<float>& out) const;
    int getWaveformBufferSize() const;

    float estimatePitchHz();

    TrackId createTrack(const std::string& name = "Track", int channels = 2, InputSource input = {}, TrackId outputBus = kMasterBusId);

    bool removeTrack(TrackId id);
    Track* getTrack(TrackId id);
    const Track* getTrack(TrackId id) const;

    std::vector<TrackId> getTrackIds() const;
    int getTrackCount() const;

    BusId createBus(const std::string& name = "Bus", int channels = 2);
    bool removeBus(BusId id);
    Bus* getBus(BusId id);
    const Bus* getBus(BusId id) const;
    Bus* getMasterBus();

    EffectInfo addGainPanEffect(TrackId trackId = kInvalidTrackId);
    EffectInfo addDistortionEffect(TrackId trackId = kInvalidTrackId);

    EffectInfo addGainPanEffect();
    EffectInfo addDistortionEffect();

    void setNodeBypass(uint64_t nodeId, bool bypass);
    bool getNodeBypass(uint64_t nodeId) const;

    AudioChain* getChain();

    bool loadVST3Plugin(const std::wstring& path, TrackId trackId = kInvalidTrackId);
    bool openPluginEditor(int index);
    void closePluginEditor(int index);
    VST3Host* getVST3Host() { return m_vst3Host.get(); }

    ChannelRack* getChannelRack() { return m_channelRack.get(); }

    ChannelId addGeneratorChannel(const std::string& name, std::unique_ptr<Generator> gen, TrackId outputTrack = kInvalidTrackId) {
        if (!m_channelRack) return kInvalidChannelId;
        return m_channelRack->addChannel(name, std::move(gen), outputTrack);
    }

    bool loadVST3PluginSmart(const std::wstring& path, TrackId targetTrack = kInvalidTrackId);

    ChannelId loadVST3Instrument(const std::wstring& path, const std::string& channelName = "", TrackId outputTrack = kInvalidTrackId);

    bool loadVST3Effect(const std::wstring& path, TrackId trackId = kInvalidTrackId);
    bool probeVST3Plugin(const std::wstring& path, std::string& outName, bool& outIsInstrument);

private:
    void processBlock(const AudioBuffer& input, AudioBuffer& output, int frames, const PC& ctx);

    AudioProcessor* findNodeGlobal(uint64_t nodeId) const;

    AudioChain* getTargetChain(TrackId trackId);

    EngineConfig m_config;
    std::unique_ptr<AudioDeviceIO> m_device;
    std::unique_ptr<MidiDeviceIO> m_midiDevice;
    std::unique_ptr<VST3Host> m_vst3Host;
    std::unique_ptr<ChannelRack> m_channelRack;
    std::vector<std::unique_ptr<Track>> m_tracks;
    std::vector<std::unique_ptr<Bus>> m_buses;
    std::atomic<uint64_t> m_nextId{2};

    std::vector<MidiEvent> m_midiStorage;
    MidiBuffer m_midiScratch{};

    LevelMeter m_outputMeter;
    WaveformRingBuffer m_waveform;

    std::atomic<bool>  m_bypassEnabled{false};

    std::unordered_map<uint64_t, ParamHandleList> m_paramMap;
    std::atomic<uint64_t> m_nextNodeId{1};

    static constexpr float MIN_PITCH_HZ = 70.0f;
    static constexpr float MAX_PITCH_HZ = 1000.0f;
};