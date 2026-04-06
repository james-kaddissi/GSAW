#include "audio/AudioLayer.hpp"
#include "audio/ChannelRack.hpp"
#include "components/ChannelRackView.hpp"
#include "audio/library/generators/SubtractiveSynth.hpp"
#include "components/BrowserPanel.hpp"
#include "components/MixerView.hpp"
#include "audio/Ids.hpp"
#include <ui/core/UILayer.hpp>
#include <ui/UIBuilder.hpp>
#include <ui/style/StyleState.hpp>
#include <audio/core/AudioConfig.hpp>
#include <audio/plugins/VST3HostPlatform.hpp>
#include <core/Layer.hpp>
#include <core/Application.hpp>

#include <iostream>
#include <memory>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace ui = gs::ui;
using gs::ui::core::UIScaleMode;
using gs::ui::widgets::UITextElement;
using gs::ui::widgets::UIWaveformElement;
using gs::ui::widgets::UIWaveformSource;
namespace StyleStates = gs::ui::style::StyleStates;
using gs::ui::style::StyleOverrides;
using gs::ui::core::DragPayload;

using namespace gs::audio;

class GSAWAppLayer final : public gs::core::Layer::GLayer {
public:
    explicit GSAWAppLayer(gs::ui::core::UILayer& uiLayer, AudioLayer& audioLayer)
        : m_uiLayer(uiLayer), m_audio(audioLayer) {}

    void onBypassClicked() {
        m_audio.toggleBypass();
        bool active = !m_audio.isBypassed();
        statusText->text.set(active ? "Active" : "Bypassed");

        if (active) statusText->removeState(StyleStates::Muted);
        else statusText->addState(StyleStates::Muted);
    }

    void onOpenAllEditors() {
        if (m_audio.getVST3Host()) m_audio.getVST3Host()->OpenAllPluginEditors(m_owner);
    }

    void onAttach(gs::core::Application::AppContext& ctx) override {
        m_owner = ctx.window.getNativeHandle();
        auto& window = ctx.window;

        m_mixerView = MixerView::create(m_audio.getEngine());

        setupChannelRack();
        m_channelRackView = ChannelRackView::create(m_audio.getEngine(), m_audio.getEngine()->getChannelRack());

        auto probeFn = [this](const std::wstring& path, std::string& outName, bool& outIsInstrument) -> bool {
            auto* engine = m_audio.getEngine();
            if (!engine) return false;
            return engine->probeVST3Plugin(path, outName, outIsInstrument);
        };

        m_browserPanel = BrowserPanel::create(m_owner, m_audio.getEngine(), std::move(probeFn));

        m_uiLayer.getLayoutRoot().setOnDrop([this](DragPayload p) {
            m_browserPanel->handleDrop(std::move(p));
        });
        fpsText = ui::Text("FPS: --").size(14).color({0.7f, 0.7f, 0.7f, 1});
        statusText = ui::Text("Active")
            .size(14)
            .color({0.5f, 0.9f, 0.5f, 1.0f})
            .style(StyleStates::Muted,
                   gs::ui::style::StyleOverrides::TextColor({0.9f, 0.4f, 0.4f, 1.0f}));

        auto audioProvider = [this](UIWaveformSource src, int count, std::vector<float>& out) {
            if (!m_audio.isInitialized()) { out.clear(); return; }
            m_audio.getRecentWaveform(count, out);
        };

        auto inputWaveform = ui::Waveform()
            .width(200).height(200)
            .source(UIWaveformSource::Input)
            .color({0.0f, 1.0f, 0.5f, 1.0f})
            .bg({0.08f, 0.08f, 0.08f, 1.0f})
            .borderColor({1.0f, 1.0f, 1.0f, 1.0f})
            .borderThickness(1)
            .radius(4.0f)
            .provider(audioProvider);

        auto titlebar = ui::HStack()
            .height(32)
            .mainAlign(gs::ui::core::UIMainAxisAlignment::SpaceBetween)
            .crossAlign(gs::ui::core::UICrossAxisAlignment::Center);

        auto leftside = ui::HStack()
            .height(32)
            .spacing(8)
            .crossAlign(gs::ui::core::UICrossAxisAlignment::Center)
                .add(ui::Text("GSAW").size(24).color({0, 1, 1, 1}))
                .add(fpsText)
                .add(statusText);

        auto buttonRow = ui::HStack().spacing(0).height(32);

        auto btnMin = ui::Button("_")
            .width(46).height(32)
            .colors({0.15f,0.15f,0.15f,1}, {0.25f,0.25f,0.25f,1}, {0.1f,0.1f,0.1f,1})
            .onClick([&window]() { window.minimize(); });

        auto btnMax = ui::Button("[ ]")
            .width(46).height(32)
            .colors({0.15f,0.15f,0.15f,1}, {0.25f,0.25f,0.25f,1}, {0.1f,0.1f,0.1f,1})
            .onClick([&window]() { window.toggleMaximize(); });

        auto btnClose = ui::Button("X")
            .width(46).height(32)
            .colors({0.15f,0.15f,0.15f,1}, {0.8f,0.2f,0.2f,1}, {0.6f,0.1f,0.1f,1})
            .onClick([&window]() { window.close(); });

        buttonRow.add(btnMin).add(btnMax).add(btnClose);
        titlebar.add(leftside).add(buttonRow);

        auto channelRackWidget = m_channelRackView->widget();
        channelRackWidget->flexGrow = 1.0f;
        channelRackWidget->heightMode = gs::ui::core::UISizeMode::Flex;

        auto mixerWidget = m_mixerView->widget();
        mixerWidget->flexGrow = 1.0f;
        mixerWidget->heightMode = gs::ui::core::UISizeMode::Flex;

        auto rightColumn = ui::VStack().spacing(4).pad(0)
            .add(ui::HStack().spacing(8).height(32)
                .add(ui::Button("Bypass").size(12).width(80).height(28)
                    .colors({0.3f,0.4f,0.5f,1}, {0.4f,0.5f,0.6f,1}, {0.2f,0.3f,0.4f,1})
                    .onClick(this, &GSAWAppLayer::onBypassClicked))
                .add(ui::Button("Open Editors").size(12).width(110).height(28)
                    .colors({0.5f,0.3f,0.5f,1}, {0.6f,0.4f,0.6f,1}, {0.4f,0.2f,0.4f,1})
                    .onClick(this, &GSAWAppLayer::onOpenAllEditors)))

            .add(channelRackWidget)
            .add(mixerWidget)

            .build();
        rightColumn->crossAxisAlignment = gs::ui::core::UICrossAxisAlignment::Stretch;
        rightColumn->flexGrow   = 1.0f;
        rightColumn->heightMode = gs::ui::core::UISizeMode::Fill;

        auto browserWidget = m_browserPanel->widget();
        browserWidget->flexGrow = 1.0f;
        browserWidget->heightMode = gs::ui::core::UISizeMode::Flex;
        auto leftColumn = ui::VStack().spacing(0).pad(0)
            .width(200)
            .add(browserWidget)
            .add(inputWaveform)
            .build();
        leftColumn->crossAxisAlignment = gs::ui::core::UICrossAxisAlignment::Stretch;
        leftColumn->heightMode = gs::ui::core::UISizeMode::Fill;

        auto mainContent = ui::HStack()
            .spacing(4)
            .pad(0)
            .add(leftColumn)
            .add(rightColumn)
            .build();
        mainContent->crossAxisAlignment = gs::ui::core::UICrossAxisAlignment::Stretch;
        mainContent->flexGrow = 1.0f;
        mainContent->heightMode = gs::ui::core::UISizeMode::Fill;

        auto root = ui::VStack().spacing(4).pad(0)
            .add(titlebar)
            .add(mainContent)
            .build();

        root->crossAxisAlignment = gs::ui::core::UICrossAxisAlignment::Stretch;

        m_uiLayer.setRoot(root);

        frameCount = 0;
        fpsTimer   = 0.0f;
        ctx.requestRender();
    }

    void onEvent(gs::core::Application::AppContext& ctx,
                 const gs::core::Application::AppEvent& e) override {
        if (e.type == gs::core::Application::AppEvent::Type::Resize)
            ctx.requestRender();
    }

    void onUpdate(gs::core::Application::AppContext& ctx, float dt) override {
        fpsTimer += dt;
        frameCount++;
        if (fpsTimer >= 3.0f) {
            float fps = static_cast<float>(frameCount) / fpsTimer;
            frameCount = 0;
            fpsTimer   = 0.0f;
            std::ostringstream ss;
            ss << "FPS: " << std::fixed << std::setprecision(0) << fps;
            if (fpsText) fpsText->text.set(ss.str());
        }
    }

private:
    void setupChannelRack() {
        auto* engine = m_audio.getEngine();
        if (!engine) return;

        auto* rack = engine->getChannelRack();
        if (!rack) return;

        TrackId synthTrackId = engine->createTrack(
            "Synth", 2, {gs::audio::InputSource::Kind::None, 0, 0},
            kMasterBusId);

        auto synth = std::make_unique<SubtractiveSynth>(16);
        ChannelId chId = rack->addChannel("Lead", std::move(synth), synthTrackId);

        PatternId patId = rack->createPattern("Arpeggio", 2);
        auto* pat = rack->getPattern(patId);
        if (pat) {
            int ppqn = kDefaultPPQN;

            pat->addNote(0  * ppqn / 2, 60, 100, ppqn / 2);
            pat->addNote(1  * ppqn / 2, 64,  90, ppqn / 2);
            pat->addNote(2  * ppqn / 2, 67,  90, ppqn / 2);
            pat->addNote(3  * ppqn / 2, 72, 100, ppqn / 2);
            pat->addNote(4  * ppqn / 2, 76,  85, ppqn / 2);
            pat->addNote(5  * ppqn / 2, 72,  90, ppqn / 2);
            pat->addNote(6  * ppqn / 2, 67,  90, ppqn / 2);
            pat->addNote(7  * ppqn / 2, 64,  85, ppqn / 2);
            pat->addNote(8  * ppqn / 2, 62,  95, ppqn / 2);
            pat->addNote(9  * ppqn / 2, 65,  90, ppqn / 2);
            pat->addNote(10 * ppqn / 2, 69,  90, ppqn / 2);
            pat->addNote(11 * ppqn / 2, 72, 100, ppqn / 2);
            pat->addNote(12 * ppqn / 2, 69,  85, ppqn / 2);
            pat->addNote(13 * ppqn / 2, 65,  90, ppqn / 2);
            pat->addNote(14 * ppqn / 2, 62,  90, ppqn / 2);
            pat->addNote(15 * ppqn / 2, 60,  95, ppqn / 2);
        }

        rack->setChannelPattern(chId, patId);
        rack->setBpm(128.0);
    }

    gs::ui::core::UILayer&              m_uiLayer;
    AudioLayer&  m_audio;
    PlatformWindowHandle                m_owner = nullptr;

    std::shared_ptr<MixerView>       m_mixerView;
    std::shared_ptr<ChannelRackView>  m_channelRackView;
    std::shared_ptr<BrowserPanel>     m_browserPanel;

    std::shared_ptr<UITextElement> fpsText, statusText;

    int   frameCount = 0;
    float fpsTimer   = 0.0f;
};

int main() {
    try {
        gs::core::Application::AppConfig cfg;
        cfg.windowDesc = gs::window::Window::WindowDesc{"GSAW", 1280, 720, false, true, 32};
#ifdef _WIN32
        cfg.fontPath            = "C:/Windows/Fonts/arial.ttf";
#else
        cfg.fontPath            = "/Library/Fonts/SF-Pro.ttf";
#endif
        cfg.uiScaleMode = UIScaleMode::Fixed;
        cfg.continuousRendering = true;

        gs::core::Application::Application app(cfg);

        auto uiLayer = std::make_unique<gs::ui::core::UILayer>();
        auto* uiPtr  = uiLayer.get();
        app.pushLayer(std::move(uiLayer));

        gs::audio::EngineConfig audioConfig;
        #ifdef _WIN32
        audioConfig.device.hostApi    = gs::audio::DeviceConfig::HostApi::ASIO;
        audioConfig.device.deviceName = "";
        #else
        audioConfig.device.hostApi    = gs::audio::DeviceConfig::HostApi::CoreAudio;
        audioConfig.device.deviceName = "";
        #endif
        audioConfig.device.sampleRate     = 48000;
        audioConfig.device.blockSize      = 256;
        audioConfig.device.inputChannels  = 2;
        audioConfig.device.outputChannels = 2;
        audioConfig.createDefaultTrack    = false;
        audioConfig.midi.deviceName       = "keystep";
        auto audioLayer = std::make_unique<AudioLayer>();
        audioLayer->configure(audioConfig);
        auto* audioPtr = audioLayer.get();
        app.pushLayer(std::move(audioLayer));
        app.pushLayer(std::make_unique<GSAWAppLayer>(*uiPtr, *audioPtr));
        return app.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}