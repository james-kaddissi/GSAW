#pragma once

#include <audio/dsp/generators/BaseGenerator.hpp>

using gs::audio::Voice;
using gs::audio::GeneratorBase;
using PC = gs::audio::ProcessContext;

class SubtractiveSynth final : public GeneratorBase {
public:
    explicit SubtractiveSynth(int voices = 16);

    const char* name() const override;
    bool hasTail() const override;

    void prepare(const PC& ctx) override;
    void reset() override;

protected:
    void onNoteOn(Voice& v, const PC& ctx) override;
    void onNoteOff(Voice& v, const PC& ctx) override;
    float renderVoice(Voice& v, const PC& ctx) override;
    bool advanceVoice(Voice& v, const PC& ctx) override;

private:
    double m_sr = 48000.0;

    static float midiNoteToHz(int note);
    static float oscillator(double phase, int waveform);
    static float polyBLEP(float t, float dt = 0.001f);
    static float sawBLEP(float phase);
    static float squareBLEP(float phase);

    float svfLowpass(float input, float cutoffHz, float res, float& ic1eq, float& ic2eq) const;
};