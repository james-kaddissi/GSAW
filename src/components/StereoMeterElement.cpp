#include "components/StereoMeterElement.hpp"

#include <algorithm>

gs::ui::core::UIGradient StereoMeterConfig::buildMeterGradient() const {
    float normYellow = (yellowDb - dbMin) / (dbMax - dbMin);
    float normRed = (redDb - dbMin) / (dbMax - dbMin);

    return gs::ui::core::UIGradient::build(gs::ui::core::UIGradientDirection::Vertical)
        .stop(0.0f, redColor)
        .stop(1.0f - normRed, redColor)
        .stop(1.0f - normRed, yellowColor)
        .stop(1.0f - normYellow, yellowColor)
        .stop(1.0f - normYellow, greenColor)
        .stop(1.0f, greenColor)
        .done();
}

StereoMeterElement::StereoMeterElement(StereoMeterConfig cfg)
    : m_cfg(std::move(cfg)) {
    preferredWidth = m_cfg.width;
    preferredHeight = m_cfg.height;
    widthMode = gs::ui::core::UISizeMode::Fixed;
    heightMode = gs::ui::core::UISizeMode::Fixed;
    m_meterGradient = m_cfg.buildMeterGradient();
}

void StereoMeterElement::setMeterSource(const LevelMeter* meter) {
    m_meter = meter;
}

StereoMeterConfig& StereoMeterElement::config() {
    return m_cfg;
}

const StereoMeterConfig& StereoMeterElement::config() const {
    return m_cfg;
}

void StereoMeterElement::rebuildGradient() {
    m_meterGradient = m_cfg.buildMeterGradient();
}

gs::ui::core::UISize StereoMeterElement::measure(const gs::ui::core::UISize& /*available*/) {
    desired = {m_cfg.width, m_cfg.height};
    return desired;
}

void StereoMeterElement::emit(gs::ui::core::UICanvas& canvas) {
    if (!visible) return;

    const float x = arranged.x;
    const float y = arranged.y;
    const float w = arranged.width;
    const float h = arranged.height;

    gs::ui::core::emitFilledRect(canvas, {x, y, w, h}, m_cfg.bgColor, 2.0f);

    if (!m_meter) return;

    float rmsL = m_meter->getRmsDb(0);
    float rmsR = m_meter->getRmsDb(1);
    float peakL = m_meter->getPeakDb(0);
    float peakR = m_meter->getPeakDb(1);

    updatePeakHold(0, peakL);
    updatePeakHold(1, peakR);

    float barAreaW = m_cfg.barWidth * 2.0f + m_cfg.barGap;
    float contentStartX = x + (w - barAreaW) * 0.5f;

    float leftBarX = contentStartX;
    float rightBarX = contentStartX + m_cfg.barWidth + m_cfg.barGap;

    float barTop = y + m_cfg.barPadTop;
    float barBottom = y + h - m_cfg.barPadBottom;
    float barH = barBottom - barTop;

    if (barH <= 0.0f) return;

    gs::ui::core::emitFilledRect(
        canvas,
        {leftBarX, barTop, m_cfg.barWidth, barH},
        m_cfg.barBgColor,
        1.0f
    );

    gs::ui::core::emitFilledRect(
        canvas,
        {rightBarX, barTop, m_cfg.barWidth, barH},
        m_cfg.barBgColor,
        1.0f
    );

    emitBar(canvas, leftBarX, barTop, barH, rmsL, true);
    emitBar(canvas, rightBarX, barTop, barH, rmsR, true);

    emitPeakLine(canvas, leftBarX, barTop, barH, m_peakHoldDb[0]);
    emitPeakLine(canvas, rightBarX, barTop, barH, m_peakHoldDb[1]);

    requestRender();
}

float StereoMeterElement::dbToNorm(float db) const {
    return std::clamp((db - m_cfg.dbMin) / (m_cfg.dbMax - m_cfg.dbMin), 0.0f, 1.0f);
}

float StereoMeterElement::computeEmissive(float db) const {
    if (m_cfg.emissiveIntensity <= 0.0f) return 0.0f;

    float base = m_cfg.emissiveIntensity;

    if (db >= m_cfg.redDb) {
        float redFrac = std::clamp(
            (db - m_cfg.redDb) / (m_cfg.dbMax - m_cfg.redDb),
            0.0f,
            1.0f
        );
        base += m_cfg.emissiveRedBoost * redFrac;
    }

    return base;
}

void StereoMeterElement::emitBar(gs::ui::core::UICanvas& canvas,
                                 float barX,
                                 float barTop,
                                 float barH,
                                 float db,
                                 bool emissive) const {
    float norm = dbToNorm(db);
    if (norm <= 0.0f) return;

    float filledH = barH * norm;
    float filledTop = barTop + barH - filledH;

    {
        gs::ui::core::UINode scissorPush{};
        scissorPush.type = gs::ui::core::UINodeType::ScissorPush;
        scissorPush.x = barX;
        scissorPush.y = filledTop;
        scissorPush.rectWidth = m_cfg.barWidth;
        scissorPush.rectHeight = filledH;
        canvas.nodes.push_back(scissorPush);
    }

    {
        gs::ui::core::UINode node{};
        node.type = gs::ui::core::UINodeType::GradientRect;
        node.x = barX;
        node.y = barTop;
        node.rectWidth = m_cfg.barWidth;
        node.rectHeight = barH;
        node.gradient = m_meterGradient;
        node.cornerRadius = 1.0f;
        node.emissiveIntensity = emissive ? computeEmissive(db) : 0.0f;
        canvas.nodes.push_back(node);
    }

    {
        gs::ui::core::UINode scissorPop{};
        scissorPop.type = gs::ui::core::UINodeType::ScissorPop;
        canvas.nodes.push_back(scissorPop);
    }
}

void StereoMeterElement::emitPeakLine(gs::ui::core::UICanvas& canvas,
                                      float barX,
                                      float barTop,
                                      float barH,
                                      float peakDb) const {
    if (peakDb <= m_cfg.dbMin) return;

    float norm = dbToNorm(peakDb);
    float lineY = barTop + barH * (1.0f - norm);

    gs::ui::core::UINode node{};
    node.type = gs::ui::core::UINodeType::FilledRect;
    node.x = barX;
    node.y = lineY - m_cfg.peakHoldWidth * 0.5f;
    node.rectWidth = m_cfg.barWidth;
    node.rectHeight = m_cfg.peakHoldWidth;
    node.color = m_cfg.peakLineColor;
    node.cornerRadius = 0.0f;

    if (m_cfg.emissivePeakHold) {
        node.emissiveIntensity = computeEmissive(peakDb) * 0.7f;
    }

    canvas.nodes.push_back(node);
}

void StereoMeterElement::updatePeakHold(int ch, float currentPeakDb) {
    auto now = std::chrono::steady_clock::now();

    if (currentPeakDb > m_peakHoldDb[ch]) {
        m_peakHoldDb[ch] = currentPeakDb;
        m_peakHoldTime[ch] = now;
    } else {
        float elapsed = std::chrono::duration<float>(now - m_peakHoldTime[ch]).count();
        if (elapsed > m_cfg.peakHoldTime) {
            float decay = m_cfg.peakFalloff * (elapsed - m_cfg.peakHoldTime);
            m_peakHoldDb[ch] = std::max(m_peakHoldDb[ch] - decay, m_cfg.dbMin);
        }
    }
}