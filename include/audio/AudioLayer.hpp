#pragma once

#include <core/Application.hpp>
#include <audio/core/AudioConfig.hpp>
#include <audio/core/Track.hpp>
#include <audio/core/Bus.hpp>
#include <audio/plugins/VST3HostPlatform.hpp>

#include "audio/EffectBinding.hpp"
#include "audio/AudioEngine.hpp"
#include "components/EffectStrip.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif

class AudioLayer final : public gs::core::Layer::GLayer {
public:
    void configure(EngineConfig config);

    void onAttach(gs::core::Application::AppContext& ctx) override;
    void onDetach(gs::core::Application::AppContext& ctx) override;
    void onUpdate(gs::core::Application::AppContext& ctx, float dt) override;
    void onRender(gs::core::Application::AppContext& ctx) override;

    bool isInitialized() const;

    float getRmsDb() const;
    float getPeakDb() const;

    void getRecentWaveform(int numSamples, std::vector<float>& out) const;

    int getSampleRate() const;
    int getWaveformBufferSize() const;

    float estimatePitchHz() const;

    void toggleBypass();
    void setBypass(bool enabled);
    bool isBypassed() const;

    void toggleNodeBypass(uint64_t nodeId);
    bool isNodeBypassed(uint64_t nodeId) const;

    TrackId createTrack(const std::string& name = "Track", int channels = 2, InputSource input = {}, TrackId outputBus = kMasterBusId);
    bool removeTrack(TrackId id);
    Track* getTrack(TrackId id);
    std::vector<TrackId> getTrackIds() const;
    int getTrackCount() const;

    BusId createBus(const std::string& name = "Bus", int channels = 2);
    Bus* getBus(BusId id);
    Bus* getMasterBus();

    std::shared_ptr<EffectStrip> createEffectStrip(EffectBinding binding, EffectStripConfig cfg = {});

    std::optional<std::wstring> pickPluginPathVST3(PlatformWindowHandle owner);

    bool loadVST3Plugin(const std::wstring& path);
    bool openPluginEditor(int index);
    void closePluginEditor(int index);

    EffectBinding addGainPanEffect(TrackId trackId);
    EffectBinding addDistortionEffect(TrackId trackId);

    VST3Host* getVST3Host();
    AudioEngine* getEngine();
    ChannelRack* getChannelRack();

    bool probeVST3Plugin(const std::wstring& path, std::string& outName, bool& outIsInstrument);

private:
    EngineConfig m_config;
    std::unique_ptr<AudioEngine> m_engine;
    bool m_initialized = false;
    float m_cachedRmsDb = -96.0f;
    float m_cachedPeakDb = -96.0f;
};