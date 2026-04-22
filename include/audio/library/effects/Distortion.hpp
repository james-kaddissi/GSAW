#pragma once

#include <audio/dsp/effects/EffectBase.hpp>

class Distortion final : public gs::audio::EffectBase {
public:
    Distortion();

    const char* name() const override;
    void prepare(const gs::audio::ProcessContext& ctx) override;
    void reset() override;
    void process(gs::audio::AudioBuffer& io, const gs::audio::ProcessContext& ctx) override;

private:
    static constexpr int kOversample = 4;

    struct Biquad {
        float b0 = 1.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float z1 = 0.0f;
        float z2 = 0.0f;
    };

    static float tubeClip(float x, float bias);
    static float hardClip(float x);
    static float biquad(Biquad& f, float in);
    static void updateLP(Biquad bq[2], double freq, double sr);
    static void updateHP(Biquad bq[2], double freq, double sr);
    static void updatePeak(Biquad bq[2], double freq, double gainDb, double q, double sr);

    float processOversampled(float in, float bias, float drive, int ch);
    void resetFilters();

    double m_sr = 44100.0;
    double m_osRate = 44100.0 * kOversample;

    Biquad m_preHP[2] = {};
    Biquad m_postLP[2] = {};
    Biquad m_presence[2] = {};
    Biquad m_aaFilter[2] = {};
    Biquad m_interpFilter[2] = {};
    Biquad m_interstage[2] = {};
};