#pragma once

#include "audio/ChannelRack.hpp"
#include "audio/AudioEngine.hpp"
#include "audio/EffectBinding.hpp"
#include "components/EffectStrip.hpp"

#include <ui/UIBuilder.hpp>
#include <ui/widgets/UIScrollArea.hpp>
#include <ui/style/StyleState.hpp>

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <cstdint>

class SubSynth;
class BagpipeSynth;

namespace StyleStates = gs::ui::style::StyleStates;

struct ChannelRackViewConfig {
    float rowHeight = 36.0f;
    float nameWidth = 120.0f;
    float sliderWidth = 100.0f;
    float panSliderWidth = 60.0f;
    float routeWidth = 70.0f;
    float totalWidth = 600.0f;
    float maxHeight = 400.0f;
    float transportHeight = 36.0f;
    float rowSpacing = 1.0f;
    float sectionPad = 6.0f;

    gs::ui::core::UIColor bg {0.12f, 0.12f, 0.14f, 1.0f};
    gs::ui::core::UIColor rowBg {0.18f, 0.18f, 0.20f, 1.0f};
    gs::ui::core::UIColor rowBgAlt {0.16f, 0.16f, 0.18f, 1.0f};
    gs::ui::core::UIColor headerColor {1.0f, 1.0f, 1.0f, 1.0f};
    gs::ui::core::UIColor labelColor {0.7f, 0.7f, 0.7f, 1.0f};
    gs::ui::core::UIColor dimColor {0.4f, 0.4f, 0.4f, 1.0f};

    gs::ui::core::UIColor btnNormal {0.25f, 0.25f, 0.3f, 1.0f};
    gs::ui::core::UIColor btnHover {0.35f, 0.35f, 0.4f, 1.0f};
    gs::ui::core::UIColor btnPress {0.18f, 0.18f, 0.22f, 1.0f};
    gs::ui::core::UIColor muteActive {0.8f, 0.3f, 0.3f, 1.0f};
    gs::ui::core::UIColor armedActive {0.9f, 0.6f, 0.1f, 1.0f};

    gs::ui::core::UIColor focusBorder {0.3f, 0.6f, 1.0f, 1.0f};
    float focusBorderW = 3.0f;

    gs::ui::core::UIColor playColor {0.2f, 0.6f, 0.3f, 1.0f};
    gs::ui::core::UIColor playHover {0.3f, 0.7f, 0.4f, 1.0f};
    gs::ui::core::UIColor stopColor {0.6f, 0.2f, 0.2f, 1.0f};
    gs::ui::core::UIColor stopHover {0.7f, 0.3f, 0.3f, 1.0f};

    gs::ui::core::UIColor addBtnNormal {0.2f, 0.4f, 0.3f, 1.0f};
    gs::ui::core::UIColor addBtnHover {0.3f, 0.5f, 0.4f, 1.0f};
    gs::ui::core::UIColor addBtnPress {0.15f, 0.3f, 0.2f, 1.0f};

    gs::ui::core::UIColor removeBtnNorm {0.5f, 0.2f, 0.2f, 1.0f};
    gs::ui::core::UIColor removeBtnHov {0.7f, 0.3f, 0.3f, 1.0f};
    gs::ui::core::UIColor removeBtnPress {0.4f, 0.15f, 0.15f, 1.0f};

    gs::ui::core::UIColor editBtnNormal {0.25f, 0.3f, 0.45f, 1.0f};
    gs::ui::core::UIColor editBtnHover {0.35f, 0.4f, 0.55f, 1.0f};
    gs::ui::core::UIColor editBtnPress {0.18f, 0.22f, 0.35f, 1.0f};

    gs::ui::core::UIColor scrollTrackColor {0.15f, 0.15f, 0.15f, 0.5f};
    gs::ui::core::UIColor scrollThumbColor {0.4f, 0.4f, 0.4f, 0.7f};
    gs::ui::core::UIColor scrollThumbHover {0.55f, 0.55f, 0.55f, 0.9f};
    gs::ui::core::UIColor scrollThumbDrag {0.65f, 0.65f, 0.65f, 1.0f};

    gs::ui::core::UIColor editorBg {0.12f, 0.12f, 0.14f, 0.97f};
    gs::ui::core::UIColor editorBorder {0.35f, 0.35f, 0.4f, 1.0f};
    gs::ui::core::UIColor editorHandleBg {0.18f, 0.18f, 0.22f, 1.0f};
    gs::ui::core::UIColor closeBtnNormal {0.5f, 0.2f, 0.2f, 1.0f};
    gs::ui::core::UIColor closeBtnHover {0.7f, 0.3f, 0.3f, 1.0f};
    gs::ui::core::UIColor closeBtnPress {0.4f, 0.15f, 0.15f, 1.0f};

    float fontSize = 11.0f;
    float smallFontSize = 10.0f;
    float buttonWidth = 28.0f;
    float editorWidth = 340.0f;
    float editorStagger = 24.0f;
    float editorHandleH = 28.0f;
    float editorOffsetX = 100.0f;
    float editorOffsetY = 60.0f;
    float stripRadius = 0.0f;
};

class ChannelRackView {
public:
    static std::shared_ptr<ChannelRackView> create(
        AudioEngine* engine,
        ChannelRack* rack,
        ChannelRackViewConfig cfg = {}
    );

    std::shared_ptr<gs::ui::widgets::UIStackPanel> widget() const;

    ChannelRackViewConfig& config();
    const ChannelRackViewConfig& config() const;

    void forceRebuild();

private:
    struct ChannelUiEntry {
        std::shared_ptr<gs::ui::widgets::UIButtonElement> muteBtn;
        std::shared_ptr<gs::ui::widgets::UIButtonElement> armedBtn;
        std::shared_ptr<gs::ui::widgets::UIStackPanel> rowPanel;
    };

    struct EditorEntry {
        std::shared_ptr<gs::ui::core::UIElement> widget;
        std::shared_ptr<EffectStrip> strip;
    };

    explicit ChannelRackView(AudioEngine* engine, ChannelRack* rack, ChannelRackViewConfig cfg);

    bool needsRebuild() const;
    void syncStates();
    void rebuild();

    std::shared_ptr<gs::ui::core::UIElement> buildTransportBar();
    std::shared_ptr<gs::ui::core::UIElement> buildChannelRow(Channel* ch, bool alt);
    std::shared_ptr<gs::ui::core::UIElement> buildAddChannelRow();

    void openGeneratorEditor(ChannelId chId);
    void closeGeneratorEditor(ChannelId chId);

    AudioEngine* m_engine;
    ChannelRack* m_rack;
    ChannelRackViewConfig m_cfg;
    std::shared_ptr<gs::ui::widgets::UIStackPanel> m_root;

    int m_lastChannelCount = -1;
    ChannelId m_lastFocusedId = kInvalidChannelId;

    std::unordered_map<ChannelId, ChannelUiEntry> m_channelUi;
    std::unordered_map<ChannelId, EditorEntry> m_openEditors;
};