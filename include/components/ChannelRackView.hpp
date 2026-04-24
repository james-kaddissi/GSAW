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

class SubtractiveSynth;
class BagpipeSynth;

namespace StyleStates = gs::ui::StyleStates;

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

    gs::ui::UIColor bg {0.12f, 0.12f, 0.14f, 1.0f};
    gs::ui::UIColor rowBg {0.18f, 0.18f, 0.20f, 1.0f};
    gs::ui::UIColor rowBgAlt {0.16f, 0.16f, 0.18f, 1.0f};
    gs::ui::UIColor headerColor {1.0f, 1.0f, 1.0f, 1.0f};
    gs::ui::UIColor labelColor {0.7f, 0.7f, 0.7f, 1.0f};
    gs::ui::UIColor dimColor {0.4f, 0.4f, 0.4f, 1.0f};

    gs::ui::UIColor btnNormal {0.25f, 0.25f, 0.3f, 1.0f};
    gs::ui::UIColor btnHover {0.35f, 0.35f, 0.4f, 1.0f};
    gs::ui::UIColor btnPress {0.18f, 0.18f, 0.22f, 1.0f};
    gs::ui::UIColor muteActive {0.8f, 0.3f, 0.3f, 1.0f};
    gs::ui::UIColor armedActive {0.9f, 0.6f, 0.1f, 1.0f};

    gs::ui::UIColor focusBorder {0.3f, 0.6f, 1.0f, 1.0f};
    float focusBorderW = 3.0f;

    gs::ui::UIColor playColor {0.2f, 0.6f, 0.3f, 1.0f};
    gs::ui::UIColor playHover {0.3f, 0.7f, 0.4f, 1.0f};
    gs::ui::UIColor stopColor {0.6f, 0.2f, 0.2f, 1.0f};
    gs::ui::UIColor stopHover {0.7f, 0.3f, 0.3f, 1.0f};

    gs::ui::UIColor addBtnNormal {0.2f, 0.4f, 0.3f, 1.0f};
    gs::ui::UIColor addBtnHover {0.3f, 0.5f, 0.4f, 1.0f};
    gs::ui::UIColor addBtnPress {0.15f, 0.3f, 0.2f, 1.0f};

    gs::ui::UIColor removeBtnNorm {0.5f, 0.2f, 0.2f, 1.0f};
    gs::ui::UIColor removeBtnHov {0.7f, 0.3f, 0.3f, 1.0f};
    gs::ui::UIColor removeBtnPress {0.4f, 0.15f, 0.15f, 1.0f};

    gs::ui::UIColor editBtnNormal {0.25f, 0.3f, 0.45f, 1.0f};
    gs::ui::UIColor editBtnHover {0.35f, 0.4f, 0.55f, 1.0f};
    gs::ui::UIColor editBtnPress {0.18f, 0.22f, 0.35f, 1.0f};

    gs::ui::UIColor scrollTrackColor {0.15f, 0.15f, 0.15f, 0.5f};
    gs::ui::UIColor scrollThumbColor {0.4f, 0.4f, 0.4f, 0.7f};
    gs::ui::UIColor scrollThumbHover {0.55f, 0.55f, 0.55f, 0.9f};
    gs::ui::UIColor scrollThumbDrag {0.65f, 0.65f, 0.65f, 1.0f};

    gs::ui::UIColor editorBg {0.12f, 0.12f, 0.14f, 0.97f};
    gs::ui::UIColor editorBorder {0.35f, 0.35f, 0.4f, 1.0f};
    gs::ui::UIColor editorHandleBg {0.18f, 0.18f, 0.22f, 1.0f};
    gs::ui::UIColor closeBtnNormal {0.5f, 0.2f, 0.2f, 1.0f};
    gs::ui::UIColor closeBtnHover {0.7f, 0.3f, 0.3f, 1.0f};
    gs::ui::UIColor closeBtnPress {0.4f, 0.15f, 0.15f, 1.0f};

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

    std::shared_ptr<gs::ui::UIStackPanel> widget() const;

    ChannelRackViewConfig& config();
    const ChannelRackViewConfig& config() const;

    void forceRebuild();

private:
    struct ChannelUiEntry {
        std::shared_ptr<gs::ui::UIButtonElement> muteBtn;
        std::shared_ptr<gs::ui::UIButtonElement> armedBtn;
        std::shared_ptr<gs::ui::UIStackPanel> rowPanel;
    };

    struct EditorEntry {
        std::shared_ptr<gs::ui::UIElement> widget;
        std::shared_ptr<EffectStrip> strip;
    };

    explicit ChannelRackView(AudioEngine* engine, ChannelRack* rack, ChannelRackViewConfig cfg);

    bool needsRebuild() const;
    void syncStates();
    void rebuild();

    std::shared_ptr<gs::ui::UIElement> buildTransportBar();
    std::shared_ptr<gs::ui::UIElement> buildChannelRow(Channel* ch, bool alt);
    std::shared_ptr<gs::ui::UIElement> buildAddChannelRow();

    void openGeneratorEditor(ChannelId chId);
    void closeGeneratorEditor(ChannelId chId);

    AudioEngine* m_engine;
    ChannelRack* m_rack;
    ChannelRackViewConfig m_cfg;
    std::shared_ptr<gs::ui::UIStackPanel> m_root;

    int m_lastChannelCount = -1;
    ChannelId m_lastFocusedId = kInvalidChannelId;

    std::unordered_map<ChannelId, ChannelUiEntry> m_channelUi;
    std::unordered_map<ChannelId, EditorEntry> m_openEditors;
};