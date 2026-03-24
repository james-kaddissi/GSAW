#pragma once

#include <ui/core/UIElement.hpp>
#include <ui/core/UITypes.hpp>
#include <audio/dsp/Metering.hpp>

#include <chrono>

using namespace gs::audio;

struct StereoMeterConfig {
    float width = 20.0f;
    float height = 100.0f;
    float barWidth = 4.0f;
    float barGap = 2.0f;
    float barPadTop = 5.0f;
    float barPadBottom = 5.0f;
    float peakHoldWidth = 1.0f;

    float dbMin = -60.0f;
    float dbMax = 6.0f;
    float yellowDb = -24.0f;
    float redDb = -8.0f;

    float peakHoldTime = 1.5f;
    float peakFalloff = 20.0f;

    float emissiveIntensity = 1.5f;
    float emissiveRedBoost = 1.0f;
    bool emissivePeakHold = true;

    gs::ui::core::UIColor bgColor {0.33f, 0.33f, 0.33f, 1.0f};
    gs::ui::core::UIColor barBgColor {0.10f, 0.10f, 0.10f, 1.0f};
    gs::ui::core::UIColor greenColor {0.15f, 0.75f, 0.30f, 1.0f};
    gs::ui::core::UIColor yellowColor {0.85f, 0.80f, 0.15f, 1.0f};
    gs::ui::core::UIColor redColor {0.90f, 0.20f, 0.15f, 1.0f};
    gs::ui::core::UIColor peakLineColor {1.00f, 1.00f, 1.00f, 0.90f};
    gs::ui::core::UIColor labelColor {0.55f, 0.55f, 0.55f, 1.0f};
    gs::ui::core::UIColor readoutColor {0.75f, 0.75f, 0.75f, 1.0f};
    gs::ui::core::UIColor scaleColor {0.35f, 0.35f, 0.35f, 1.0f};

    gs::ui::core::UIGradient buildMeterGradient() const;
};

class StereoMeterElement final : public gs::ui::core::UIElement {
public:
    explicit StereoMeterElement(StereoMeterConfig cfg = {});

    void setMeterSource(const LevelMeter* meter);

    StereoMeterConfig& config();
    const StereoMeterConfig& config() const;

    void rebuildGradient();

    gs::ui::core::UISize measure(const gs::ui::core::UISize& available) override;
    void emit(gs::ui::core::UICanvas& canvas) override;

private:
    float dbToNorm(float db) const;
    float computeEmissive(float db) const;

    void emitBar(gs::ui::core::UICanvas& canvas,
                 float barX,
                 float barTop,
                 float barH,
                 float db,
                 bool emissive = true) const;

    void emitPeakLine(gs::ui::core::UICanvas& canvas,
                      float barX,
                      float barTop,
                      float barH,
                      float peakDb) const;

    void updatePeakHold(int ch, float currentPeakDb);

private:
    StereoMeterConfig m_cfg;
    gs::ui::core::UIGradient m_meterGradient;
    const LevelMeter* m_meter = nullptr;

    float m_peakHoldDb[2] = {-96.0f, -96.0f};
    std::chrono::steady_clock::time_point m_peakHoldTime[2] = {};
};