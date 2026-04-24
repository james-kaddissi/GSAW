#include "components/EffectStrip.hpp"

#include <algorithm>
#include <cstdio>

std::shared_ptr<EffectStrip> EffectStrip::create(
    EffectBinding binding,
    std::function<void(uint64_t)> bypassToggleFn,
    EffectStripConfig cfg)
{
    auto p = std::shared_ptr<EffectStrip>(
        new EffectStrip(std::move(binding), std::move(bypassToggleFn), std::move(cfg))
    );
    p->build();
    return p;
}

EffectStrip::EffectStrip(EffectBinding binding, std::function<void(uint64_t)> bypassFn, EffectStripConfig cfg)
    : m_binding(std::move(binding))
    , m_bypassFn(std::move(bypassFn))
    , m_cfg(std::move(cfg))
    , m_root(gs::ui::UIStackPanel::create(gs::ui::UIOrientation::Vertical))
{}

std::shared_ptr<gs::ui::UIStackPanel> EffectStrip::widget() const {
    return m_root;
}

const EffectBinding& EffectStrip::binding() const { return m_binding; }
EffectStripConfig& EffectStrip::config() { return m_cfg; }
const EffectStripConfig& EffectStrip::config() const { return m_cfg; }

void EffectStrip::rebuild() {
    build();
}

void EffectStrip::build() {
    namespace ui = gs::ui;

    m_root->children.clear();
    m_root->spacing = m_cfg.rowSpacing;
    m_root->padding = gs::ui::UIThickness(m_cfg.panelPadding);

    m_root->fill = m_cfg.panelBg;
    m_root->borderColor = m_cfg.borderColor;
    m_root->borderThickness = m_cfg.borderThickness;
    m_root->cornerRadius = m_cfg.cornerRadius;

    if (m_cfg.width > 0.0f) {
        m_root->preferredWidth = m_cfg.width;
        m_root->widthMode = gs::ui::UISizeMode::Fill;
    }
    if (m_cfg.height > 0.0f) {
        m_root->preferredHeight = m_cfg.height;
        m_root->heightMode = gs::ui::UISizeMode::Fill;
    }

    const auto& info = m_binding.info();
    int paramCount = (int)info.params.size();

    float rowHeight = m_cfg.defaultRowHeight;

    if (m_cfg.height > 0.0f) {
        int totalRows = paramCount + 1;
        float usable = m_cfg.height - m_cfg.panelPadding * 2.0f - m_cfg.rowSpacing * (float)(totalRows - 1);

        float totalShares = m_cfg.headerScale + (float)paramCount;
        float shareHeight = usable / totalShares;

        rowHeight = std::max(m_cfg.minRowHeight, shareHeight);
    }

    float headerHeight = rowHeight * m_cfg.headerScale;

    float contentWidth = (m_cfg.width > 0.0f) ? m_cfg.width - m_cfg.panelPadding * 2.0f : 0.0f;

    float sliderWidth = (contentWidth > 0.0f) ? contentWidth - m_cfg.paramSpacing - m_cfg.valueLabelWidth : 200.0f;

    sliderWidth = std::max(sliderWidth, 60.0f);

    auto title = ui::Text("")
        .size(m_cfg.titleFontSize)
        .color(m_cfg.titleColor)
        .height(headerHeight);

    m_binding.bindTitle(title);

    auto headerRow = ui::HStack()
        .spacing(m_cfg.headerSpacing)
        .height(headerHeight)
        .mainAlign(gs::ui::UIMainAxisAlignment::Center)
        .crossAlign(gs::ui::UICrossAxisAlignment::Center)
        .add(title);

    if (m_cfg.showBypass) {
        float btnH = std::min(m_cfg.bypassHeight, headerHeight - 4.0f);

        auto bypassBtn = ui::Button("Bypass")
            .size(m_cfg.valueFontSize)
            .width(m_cfg.bypassWidth)
            .height(btnH)
            .colors(m_cfg.bypassNormal, m_cfg.bypassHover, m_cfg.bypassPress);

        m_binding.bindBypass(bypassBtn, m_bypassFn);
        headerRow.add(bypassBtn);
    }

    m_root->add(headerRow.build());

    for (auto& param : info.params) {
        auto slider = ui::Slider()
            .width(sliderWidth)
            .height(rowHeight);

        auto valueLabel = ui::Text("")
            .size(m_cfg.valueFontSize)
            .color(m_cfg.valueColor)
            .height(rowHeight);

        if (m_cfg.valueLabelWidth > 0.0f) {
            std::shared_ptr<gs::ui::UITextElement> labelEl = valueLabel;
            labelEl->preferredWidth = m_cfg.valueLabelWidth;
            labelEl->widthMode = gs::ui::UISizeMode::Fill;
        }

        std::string fmt = buildFormat(param);

        m_binding.bind(param.spec->id, slider);
        m_binding.bindValue(param.spec->id, valueLabel, fmt.c_str());

        auto row = ui::HStack()
            .spacing(m_cfg.paramSpacing)
            .height(rowHeight)
            .crossAlign(gs::ui::UICrossAxisAlignment::Center)
            .add(slider)
            .add(valueLabel);

        m_root->add(row.build());
    }
}

std::string EffectStrip::buildFormat(const ParamHandle& param) {
    int precision = 1;

    if (param.spec->step > 0.0f) {
        if (param.spec->step >= 1.0f) precision = 0;
        else if (param.spec->step >= 0.1f) precision = 1;
        else if (param.spec->step >= 0.01f) precision = 2;
        else precision = 3;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "%s: %%.%df", param.spec->label, precision);
    return buf;
}