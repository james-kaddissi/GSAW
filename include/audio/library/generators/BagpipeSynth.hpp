#pragma once

#include <audio/dsp/generators/BaseGenerator.hpp>

#include <vector>
#include <cstdint>

struct ChanterVoiceState {
    double phase = 0.0;

    float f1_ic1 = 0.0f;
    float f1_ic2 = 0.0f;
    float f2_ic1 = 0.0f;
    float f2_ic2 = 0.0f;
    float f3_ic1 = 0.0f;
    float f3_ic2 = 0.0f;

    float nasal_ic1 = 0.0f;
    float nasal_ic2 = 0.0f;

    float lpState = 0.0f;

    float targetFreq = 440.0f;
    float currentFreq = 440.0f;
    float velGain = 1.0f;
    int midiNote = 69;

    bool graceActive = false;
    float graceFreq = 0.0f;
    int graceSamplesLeft = 0;

    float chiffCounter = 0.0f;

    bool active = false;
    bool releasing = false;

    void reset();
};

struct DroneState {
    double phase = 0.0;
    float ic1 = 0.0f;
    float ic2 = 0.0f;
    float lp1 = 0.0f;
    float lp2 = 0.0f;
    float noiseState = 0.0f;
    float driftPhase = 0.0f;

    void reset();
};

using gs::audio::Voice;
using gs::audio::GeneratorBase;
using PC = gs::audio::ProcessContext;

class BagpipeSynth final : public GeneratorBase {
public:
    enum Params {
        pTonicNote = 0,
        pBagPressure,
        pPressureLag,
        pPressureNoise,
        pChanterDrive,
        pChanterBrightness,
        pChanterNasal,
        pChanterMix,
        pDroneTenorMix,
        pDroneBassMix,
        pDroneBrightness,
        pDroneDetune,
        pDroneNoise,
        pBodyLow,
        pBodyMid,
        pBodyHigh,
        pStereoWidth,
        pStartChiff,
        pReleaseMs,
        pGain,
        pCou
    };

    explicit BagpipeSynth(int voices = 8);

    const char* name() const override;
    bool hasTail() const override;

    void prepare(const PC& ctx) override;
    void reset() override;

    void injectGraceNote(int voiceIdx, int graceNote, int durationSamples = 0);
    void injectCut(int voiceIdx);
    void injectStrike(int voiceIdx);

protected:
    void onNoteOn(Voice& v, const PC& ctx) override;
    void onNoteOff(Voice& v, const PC& ctx) override;
    float renderVoice(Voice& v, const PC& ctx) override;
    bool advanceVoice(Voice& v, const PC& ctx) override;

private:
    static float bagpipeFreq(int midiNote, int tonicNote);
    static float midiNoteToHz(int note);
    int voiceIndex(const Voice& v) const;
    static float chanterReed(float phase, float drive);
    static float droneReed(float phase, float pw);

    float renderDrone(DroneState& ds, float freq, float pw, float brightness, float noiseAmt, float pressure);
    float applyBodyEQ(float input, float lowDb, float midDb, float highDb);
    float calcLpAlpha(float cutoffHz) const;
    float svfBandpass(float input, float cutoffHz, float Q, float& ic1eq, float& ic2eq) const;
    float cheapNoise();
    float cheapNoiseSlow();

    double m_sr = 48000.0;
    uint32_t m_noiseState = 48271;
    uint32_t m_noiseStateSlow = 91573;

    std::vector<ChanterVoiceState> m_chanterState;

    DroneState m_tenor1;
    DroneState m_tenor2;
    DroneState m_bass;

    float m_bagPressure = 0.0f;
    bool m_bagFilling = false;
    bool m_anyNoteHeld = false;

    float m_bodyLo_z1 = 0.0f;
    float m_bodyLo_z2 = 0.0f;
    float m_bodyMid_ic1 = 0.0f;
    float m_bodyMid_ic2 = 0.0f;
    float m_bodyHi_z1 = 0.0f;
    float m_bodyHi_z2 = 0.0f;
};