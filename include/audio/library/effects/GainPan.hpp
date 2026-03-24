#pragma once

#include <audio/dsp/effects/BaseEffect.hpp>

class GainPan final : public gs::audio::EffectBase {
public:
    GainPan();

    const char* name() const override;
    void prepare(const gs::audio::ProcessContext& ctx) override;
    void reset() override;
    void process(gs::audio::AudioBuffer& io, const gs::audio::ProcessContext& ctx) override;
};