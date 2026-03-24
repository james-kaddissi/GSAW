#pragma once
#include "audio/AudioEngine.hpp"
#include "components/EffectStrip.hpp"
#include "components/StereoMeterElement.hpp"
#include "audio/EffectBinding.hpp"

#include <ui/UIBuilder.hpp>
#include <ui/widgets/UIScrollArea.hpp>
#include <ui/style/StyleState.hpp>
#include <audio/core/Track.hpp>
#include <audio/core/Bus.hpp>
#include <audio/dsp/AudioChain.hpp>
#include <audio/dsp/AudioProcessor.hpp>

#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <string>
#include <cstdint>

using gs::audio::ChainItemView;

namespace StyleStates = gs::ui::style::StyleStates;

struct MixerViewConfig {
    float stripWidth = 150.0f;
    float busWidth = 150.0f;
    float stripPadding = 0.0f;
    float stripSpacing = 0.0f;
    float stripRadius = 0.0f;
    float stripBorderThickness = 1.0f;

    gs::ui::core::UIColor stripBg {0.43f, 0.43f, 0.43f, 1.0f};
    gs::ui::core::UIColor stripBorderColor {0.0f, 0.0f, 0.0f, 1.0f};
    gs::ui::core::UIColor masterStripBg {0.14f, 0.12f, 0.16f, 1.0f};
    gs::ui::core::UIColor headerColor {1.0f,  1.0f,  1.0f,  1.0f};
    gs::ui::core::UIColor labelColor {0.7f,  0.7f,  0.7f,  1.0f};
    gs::ui::core::UIColor dimColor {0.4f,  0.4f,  0.4f,  1.0f};

    gs::ui::core::UIColor btnNormal {0.25f, 0.25f, 0.3f,  1.0f};
    gs::ui::core::UIColor btnHover {0.35f, 0.35f, 0.4f,  1.0f};
    gs::ui::core::UIColor btnPress {0.18f, 0.18f, 0.22f, 1.0f};

    gs::ui::core::UIColor muteActive {0.8f,  0.3f,  0.3f,  1.0f};
    gs::ui::core::UIColor soloActive {0.9f,  0.8f,  0.2f,  1.0f};

    gs::ui::core::UIColor addBtnNormal {0.2f,  0.4f,  0.3f,  1.0f};
    gs::ui::core::UIColor addBtnHover {0.3f,  0.5f,  0.4f,  1.0f};
    gs::ui::core::UIColor addBtnPress {0.15f, 0.3f,  0.2f,  1.0f};

    gs::ui::core::UIColor removeBtnNorm {0.5f,  0.2f,  0.2f,  1.0f};
    gs::ui::core::UIColor removeBtnHov {0.7f,  0.3f,  0.3f,  1.0f};
    gs::ui::core::UIColor removeBtnPress {0.4f,  0.15f, 0.15f, 1.0f};

    gs::ui::core::UIColor editBtnNormal {0.25f, 0.3f,  0.45f, 1.0f};
    gs::ui::core::UIColor editBtnHover {0.35f, 0.4f,  0.55f, 1.0f};
    gs::ui::core::UIColor editBtnPress {0.18f, 0.22f, 0.35f, 1.0f};

    gs::ui::core::UIColor scrollTrackColor {0.15f, 0.15f, 0.15f, 0.5f};
    gs::ui::core::UIColor scrollThumbColor {0.4f,  0.4f,  0.4f,  0.7f};
    gs::ui::core::UIColor scrollThumbHover {0.55f, 0.55f, 0.55f, 0.9f};
    gs::ui::core::UIColor scrollThumbDrag {0.65f, 0.65f, 0.65f, 1.0f};

    float headerFontSize = 14.0f;
    float labelFontSize = 11.0f;
    float buttonFontSize = 12.0f;
    float fxFontSize = 10.0f;

    float faderHeight = 10.0f;
    float panHeight = 10.0f;
    float buttonHeight = 12.0f;
    float buttonWidth = 40.0f;
    float smallBtnWidth = 28.0f;
    float smallBtnHeight = 9.0f;
    float fxRowHeight = 15.0f;
    float sectionSpacing = 0.0f;
    float innerSpacing = 3.0f;

    float insertsTextHeight = 9.0f;
    float titleRowHeight = 20.0f;

    float panTextHeight = 6.0f;
    float faderTextHeight = 6.0f;

    float fxScrollMinHeight = 20.0f;
    float fxScrollbarWidth = 6.0f;

    float meterWidth = 15.0f;
    float meterMinHeight = 30.0f;

    float editorWidth = 320.0f;
    float editorHeight = 0.0f;
    float editorOffsetX = 160.0f;
    float editorOffsetY = 80.0f;
    float editorStagger = 24.0f;
    float editorHandleH = 28.0f;
    gs::ui::core::UIColor editorBg {0.12f, 0.12f, 0.14f, 0.97f};
    gs::ui::core::UIColor editorBorder {0.35f, 0.35f, 0.4f,  1.0f};
    gs::ui::core::UIColor editorHandleBg {0.18f, 0.18f, 0.22f, 1.0f};
    gs::ui::core::UIColor closeBtnNormal {0.5f,  0.2f,  0.2f,  1.0f};
    gs::ui::core::UIColor closeBtnHover {0.7f,  0.3f,  0.3f,  1.0f};
    gs::ui::core::UIColor closeBtnPress {0.4f,  0.15f, 0.15f, 1.0f};
};

class MixerView {
public:
    static std::shared_ptr<MixerView> create(AudioEngine* engine, MixerViewConfig cfg = {});

    std::shared_ptr<gs::ui::widgets::UIStackPanel> widget() const;

    MixerViewConfig&       config();
    const MixerViewConfig& config() const;

    void forceRebuild();

private:
    explicit MixerView(AudioEngine* engine, MixerViewConfig cfg);


    bool needsRebuild() const;
    void syncStates();
    void snapshotVersions();
    std::shared_ptr<StereoMeterElement> makeMeter(const LevelMeter* source);
    void rebuild();

    static int inputSourceToIndex(const InputSource& src);

    std::shared_ptr<gs::ui::core::UIElement> buildTrackStrip(Track* track);
    std::shared_ptr<gs::ui::core::UIElement> buildFxRow(const ChainItemView& item, AudioChain* chain, int index, int total);
    void openEditor(uint64_t nodeId);
    void closeEditor(uint64_t nodeId);
    void cleanupStaleEditors();
    AudioProcessor* findNodeAnywhere(uint64_t nodeId) const;
    std::shared_ptr<gs::ui::core::UIElement> buildBusStrip(Bus* bus, const std::string& label);
    std::shared_ptr<gs::ui::core::UIElement> buildAddTrackStrip();

    struct TrackUiEntry {
        std::shared_ptr<gs::ui::widgets::UIButtonElement> muteBtn;
        std::shared_ptr<gs::ui::widgets::UIButtonElement> soloBtn;
    };

    struct EditorEntry {
        std::shared_ptr<gs::ui::core::UIElement> widget;
        std::shared_ptr<EffectStrip> strip;
    };

    AudioEngine* m_engine;
    MixerViewConfig m_cfg;
    std::shared_ptr<gs::ui::widgets::UIStackPanel> m_root;

    int m_lastTrackCount = -1;
    std::vector<uint64_t> m_lastChainVersions;
    uint64_t m_lastMasterVersion = 0;

    std::unordered_map<TrackId, TrackUiEntry> m_trackUi;
    std::shared_ptr<gs::ui::widgets::UIButtonElement> m_masterMuteBtn;

    std::unordered_map<uint64_t, EffectInfo> m_effectInfoMap;
    std::unordered_map<uint64_t, EditorEntry> m_openEditors;
};