#pragma once

#include <ui/UIBuilder.hpp>
#include <audio/core/AudioTypes.hpp>
#include "audio/EffectBinding.hpp"

#include <memory>
#include <functional>
#include <cstdint>
#include <string>

struct EffectStripConfig {
    float width = 0.0f;
    float height = 0.0f;

    float cornerRadius = 8.0f;
    float borderThickness = 1.0f;
    float panelPadding = 12.0f;
    gs::ui::core::UIColor panelBg {0.14f, 0.14f, 0.16f, 1.0f};
    gs::ui::core::UIColor borderColor {0.3f,  0.3f,  0.35f, 1.0f};

    float rowSpacing = 6.0f;
    float headerSpacing = 8.0f;
    float paramSpacing = 10.0f;

    float minRowHeight = 24.0f;
    float defaultRowHeight = 30.0f;
    float headerScale = 1.3f;

    float valueLabelWidth = 120.0f;
    float valueFontSize = 12.0f;
    gs::ui::core::UIColor valueColor {0.7f, 0.7f, 0.7f, 1.0f};

    bool  showBypass = true;
    float bypassWidth = 80.0f;
    float bypassHeight = 28.0f;

    float titleFontSize = 20.0f;
    gs::ui::core::UIColor titleColor {1.0f, 1.0f, 1.0f, 1.0f};

    gs::ui::core::UIColor bypassNormal {0.4f, 0.3f, 0.3f, 1.0f};
    gs::ui::core::UIColor bypassHover {0.6f, 0.4f, 0.4f, 1.0f};
    gs::ui::core::UIColor bypassPress {0.3f, 0.2f, 0.2f, 1.0f};
};

class EffectStrip {
public:
    static std::shared_ptr<EffectStrip> create(
        EffectBinding binding,
        std::function<void(uint64_t)> bypassToggleFn,
        EffectStripConfig cfg = {}
    );

    std::shared_ptr<gs::ui::widgets::UIStackPanel> widget() const;

    const EffectBinding& binding() const;
    EffectStripConfig& config();
    const EffectStripConfig& config() const;

    void rebuild();

private:
    explicit EffectStrip(EffectBinding binding, std::function<void(uint64_t)> bypassFn, EffectStripConfig cfg);

    void build();
    static std::string buildFormat(const ParamHandle& param);

private:
    EffectBinding m_binding;
    std::function<void(uint64_t)> m_bypassFn;
    EffectStripConfig m_cfg;
    std::shared_ptr<gs::ui::widgets::UIStackPanel> m_root;
};