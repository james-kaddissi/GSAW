#include "audio/library/generators/BagpipeSynth.hpp"

#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.141592653589793238462643383279502884
#endif

static constexpr float kBagpipeCentsOffset[12] = {
    0.0f,
    -10.0f,
    4.0f,
    -12.0f,
    -2.0f,
    2.0f,
    -14.0f,
    4.0f,
    -10.0f,
    -4.0f,
    2.0f,
    -16.0f
};

void ChanterVoiceState::reset() {
    phase = 0.0;
    f1_ic1 = 0.0f;
    f1_ic2 = 0.0f;
    f2_ic1 = 0.0f;
    f2_ic2 = 0.0f;
    f3_ic1 = 0.0f;
    f3_ic2 = 0.0f;
    nasal_ic1 = 0.0f;
    nasal_ic2 = 0.0f;
    lpState = 0.0f;
    targetFreq = 440.0f;
    currentFreq = 440.0f;
    velGain = 1.0f;
    midiNote = 69;
    graceActive = false;
    graceFreq = 0.0f;
    graceSamplesLeft = 0;
    chiffCounter = 0.0f;
    active = false;
    releasing = false;
}

void DroneState::reset() {
    phase = 0.0;
    ic1 = 0.0f;
    ic2 = 0.0f;
    lp1 = 0.0f;
    lp2 = 0.0f;
    noiseState = 0.0f;
    driftPhase = 0.0f;
}

BagpipeSynth::BagpipeSynth(int voices)
    : GeneratorBase(voices)
    , m_chanterState(voices) {
    addParam({"tonic_note", "Tonic Note", 45.0f, 72.0f, 57.0f});
    addParam({"bag_pressure", "Bag Pressure", 0.0f, 1.0f, 0.85f});
    addParam({"pressure_lag", "Pressure Lag (ms)", 5.0f, 2000.0f, 200.0f});
    addParam({"pressure_noise", "Pressure Noise", 0.0f, 1.0f, 0.03f});
    addParam({"chanter_drive", "Chanter Drive", 0.05f, 0.95f, 0.45f});
    addParam({"chanter_bright", "Chanter Brightness", 1500.0f, 16000.0f, 5500.0f});
    addParam({"chanter_nasal", "Chanter Nasal", 0.0f, 1.0f, 0.6f});
    addParam({"chanter_mix", "Chanter Mix", 0.0f, 1.0f, 0.7f});
    addParam({"drone_tenor_mix", "Drone Tenor Mix", 0.0f, 1.0f, 0.35f});
    addParam({"drone_bass_mix", "Drone Bass Mix", 0.0f, 1.0f, 0.3f});
    addParam({"drone_bright", "Drone Brightness", 800.0f, 10000.0f, 2200.0f});
    addParam({"drone_detune", "Drone Detune (cents)", 0.0f, 15.0f, 3.0f});
    addParam({"drone_noise", "Drone Noise", 0.0f, 1.0f, 0.04f});
    addParam({"body_low", "Body Low", -6.0f, 6.0f, 1.5f});
    addParam({"body_mid", "Body Mid", -6.0f, 6.0f, 2.0f});
    addParam({"body_high", "Body High", -6.0f, 6.0f, -1.0f});
    addParam({"stereo_width", "Stereo Width", 0.0f, 1.0f, 0.3f});
    addParam({"start_chiff", "Start Chiff", 0.0f, 1.0f, 0.25f});
    addParam({"release_ms", "Release (ms)", 10.0f, 3000.0f, 400.0f});
    addParam({"gain", "Gain (dB)", -30.0f, 6.0f, -6.0f});
}

const char* BagpipeSynth::name() const {
    return "BagpipeSynth";
}

bool BagpipeSynth::hasTail() const {
    return true;
}

void BagpipeSynth::prepare(const PC& ctx) {
    m_sr = ctx.sampleRate;
    initSmoothing(m_sr, 5.0f);
    m_noiseState = 48271;

    m_chanterState.resize(m_voices.size());
    for (auto& cs : m_chanterState) {
        cs.reset();
    }

    m_tenor1.reset();
    m_tenor2.reset();
    m_bass.reset();

    m_bagPressure = 0.0f;
    m_bagFilling = false;
    m_anyNoteHeld = false;

    m_bodyLo_z1 = 0.0f;
    m_bodyLo_z2 = 0.0f;
    m_bodyMid_ic1 = 0.0f;
    m_bodyMid_ic2 = 0.0f;
    m_bodyHi_z1 = 0.0f;
    m_bodyHi_z2 = 0.0f;
}

void BagpipeSynth::reset() {
    for (auto& v : m_voices) {
        v.active = false;
        v.releasing = false;
        v.phase = 0.0;
        v.envelope = 0.0f;
    }

    for (auto& cs : m_chanterState) {
        cs.reset();
    }

    m_tenor1.reset();
    m_tenor2.reset();
    m_bass.reset();
    m_bagPressure = 0.0f;
    m_bagFilling = false;
    m_anyNoteHeld = false;
}

void BagpipeSynth::onNoteOn(Voice& v, const PC&) {
    int idx = voiceIndex(v);
    auto& cs = m_chanterState[idx];

    int tonicNote = static_cast<int>(paramSmoothed(pTonicNote) + 0.5f);
    float startChiff = paramSmoothed(pStartChiff);

    cs.midiNote = v.note;
    cs.targetFreq = bagpipeFreq(v.note, tonicNote);
    cs.velGain = static_cast<float>(v.velocity) / 127.0f;
    cs.active = true;
    cs.releasing = false;

    if (!m_bagFilling) {
        m_bagFilling = true;
        cs.chiffCounter = static_cast<float>(m_sr * 0.035) * startChiff;
    } else {
        cs.currentFreq = cs.targetFreq;
        cs.chiffCounter = 0.0f;
    }

    v.phase = 0.0;
    v.releasing = false;
    m_anyNoteHeld = true;
}

void BagpipeSynth::onNoteOff(Voice& v, const PC&) {
    int idx = voiceIndex(v);
    auto& cs = m_chanterState[idx];
    cs.releasing = true;
    v.releasing = true;

    m_anyNoteHeld = false;
    for (size_t i = 0; i < m_voices.size(); i++) {
        if (m_voices[i].active && !m_chanterState[i].releasing) {
            m_anyNoteHeld = true;
            break;
        }
    }

    if (!m_anyNoteHeld) {
        m_bagFilling = false;
    }
}

float BagpipeSynth::renderVoice(Voice& v, const PC&) {
    int idx = voiceIndex(v);
    auto& cs = m_chanterState[idx];

    float pressure = paramSmoothed(pBagPressure);
    float pressureNoise = paramSmoothed(pPressureNoise);
    float chanterDrive = paramSmoothed(pChanterDrive);
    float chanterBright = paramSmoothed(pChanterBrightness);
    float chanterNasal = paramSmoothed(pChanterNasal);
    float chanterMix = paramSmoothed(pChanterMix);
    float tenorMix = paramSmoothed(pDroneTenorMix);
    float bassMix = paramSmoothed(pDroneBassMix);
    float droneBright = paramSmoothed(pDroneBrightness);
    float droneDetune = paramSmoothed(pDroneDetune);
    float droneNoise = paramSmoothed(pDroneNoise);
    float bodyLowDb = paramSmoothed(pBodyLow);
    float bodyMidDb = paramSmoothed(pBodyMid);
    float bodyHighDb = paramSmoothed(pBodyHigh);
    float startChiff = paramSmoothed(pStartChiff);
    float gainDb = paramSmoothed(pGain);
    float gainLin = std::pow(10.0f, gainDb / 20.0f);
    int tonicNote = static_cast<int>(paramSmoothed(pTonicNote) + 0.5f);

    float pressTarget = m_bagFilling ? pressure : 0.0f;
    float pressLagMs = paramSmoothed(pPressureLag);
    float pressAlpha = 1.0f - std::exp(-1000.0f / (pressLagMs * static_cast<float>(m_sr)));
    m_bagPressure += pressAlpha * (pressTarget - m_bagPressure);

    float pNoise = cheapNoise() * 0.3f + cheapNoiseSlow() * 0.7f;
    float effectivePressure = m_bagPressure + pNoise * pressureNoise * m_bagPressure;
    effectivePressure = std::clamp(effectivePressure, 0.0f, 1.0f);

    if (effectivePressure < 0.0001f && cs.releasing) {
        return 0.0f;
    }

    float output = 0.0f;

    if (cs.active) {
        float freqAlpha = 1.0f - std::exp(-2.0f * static_cast<float>(M_PI) * 30.0f / static_cast<float>(m_sr));
        cs.currentFreq += freqAlpha * (cs.targetFreq - cs.currentFreq);
        float freq = cs.currentFreq;

        if (cs.graceActive && cs.graceSamplesLeft > 0) {
            freq = cs.graceFreq;
            cs.graceSamplesLeft--;
            if (cs.graceSamplesLeft <= 0) {
                cs.graceActive = false;
            }
        }

        float pitchBend = 1.0f + (effectivePressure - 0.85f) * 0.003f;
        freq *= pitchBend;

        double phaseInc = static_cast<double>(freq) / m_sr;
        float p = static_cast<float>(cs.phase);
        float driveAmount = chanterDrive * (0.8f + effectivePressure * 0.4f);
        float reed = chanterReed(p, driveAmount);

        cs.phase += phaseInc;
        if (cs.phase >= 1.0) {
            cs.phase -= 1.0;
        }

        if (cs.chiffCounter > 0.0f) {
            float chiffMax = static_cast<float>(m_sr * 0.035) * startChiff;
            float t = cs.chiffCounter / std::max(chiffMax, 1.0f);
            float chiff = cheapNoise() * 0.25f
                + std::sin(4.0f * static_cast<float>(M_PI) * p) * 0.35f
                + std::sin(7.0f * static_cast<float>(M_PI) * p) * 0.15f;
            reed += chiff * t * t * 0.35f;
            cs.chiffCounter -= 1.0f;
        }

        float nyq = static_cast<float>(m_sr * 0.45);
        float f1 = std::clamp(900.0f, 100.0f, nyq);
        float f2 = std::clamp(1500.0f, 100.0f, nyq);
        float f3 = std::clamp(2700.0f, 100.0f, nyq);

        float b1 = svfBandpass(reed, f1, 3.5f, cs.f1_ic1, cs.f1_ic2);
        float b2 = svfBandpass(reed, f2, 5.0f, cs.f2_ic1, cs.f2_ic2);
        float b3 = svfBandpass(reed, f3, 3.0f, cs.f3_ic1, cs.f3_ic2);

        float nasalBp = svfBandpass(reed, 1350.0f, 6.0f, cs.nasal_ic1, cs.nasal_ic2);

        float boreSignal = b1 * 0.55f + b2 * 1.0f + b3 * 0.4f + nasalBp * chanterNasal * 0.6f;

        float chanterSig = reed * 0.15f + boreSignal * 0.85f;

        float lpAlpha = calcLpAlpha(chanterBright);
        cs.lpState += lpAlpha * (chanterSig - cs.lpState);
        chanterSig = cs.lpState;

        output += chanterSig * chanterMix * effectivePressure;
    }

    if (idx == 0) {
        float tonicHz = midiNoteToHz(tonicNote);

        float tenor1Hz = tonicHz;
        float tenor1 = renderDrone(m_tenor1, tenor1Hz, 0.45f, droneBright, droneNoise, effectivePressure);

        float detuneFactor = std::pow(2.0f, droneDetune / 1200.0f);
        float tenor2Hz = tonicHz * detuneFactor;
        float tenor2 = renderDrone(m_tenor2, tenor2Hz, 0.48f, droneBright, droneNoise, effectivePressure);

        float bassHz = tonicHz * 0.5f;
        float bassDrone = renderDrone(m_bass, bassHz, 0.55f, droneBright * 0.6f, droneNoise * 0.7f, effectivePressure);

        float droneSig = (tenor1 + tenor2) * 0.5f * tenorMix + bassDrone * bassMix;
        output += droneSig;
    }

    if (idx == 0) {
        output = applyBodyEQ(output, bodyLowDb, bodyMidDb, bodyHighDb);
    }

    return output * effectivePressure * cs.velGain * gainLin;
}

bool BagpipeSynth::advanceVoice(Voice& v, const PC&) {
    int idx = voiceIndex(v);
    auto& cs = m_chanterState[idx];

    if (cs.releasing) {
        float releaseMs = paramSmoothed(pReleaseMs);
        float releaseAlpha = 1.0f - std::exp(-1000.0f / (releaseMs * static_cast<float>(m_sr)));
        m_bagPressure *= 1.0f - releaseAlpha * 0.1f;

        if (m_bagPressure < 0.0001f) {
            cs.active = false;
            return false;
        }
    }

    return true;
}

float BagpipeSynth::bagpipeFreq(int midiNote, int tonicNote) {
    int degree = ((midiNote - tonicNote) % 12 + 12) % 12;
    float centsOffset = kBagpipeCentsOffset[degree];
    float baseHz = midiNoteToHz(midiNote);
    return baseHz * std::pow(2.0f, centsOffset / 1200.0f);
}

float BagpipeSynth::midiNoteToHz(int note) {
    return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
}

int BagpipeSynth::voiceIndex(const Voice& v) const {
    return static_cast<int>(&v - m_voices.data());
}

float BagpipeSynth::chanterReed(float phase, float drive) {
    constexpr float twoPi = 2.0f * static_cast<float>(M_PI);

    float pw = 0.3f + drive * 0.15f;
    float warpedPhase;
    if (phase < pw) {
        warpedPhase = phase / pw * 0.5f;
    } else {
        warpedPhase = 0.5f + (phase - pw) / (1.0f - pw) * 0.5f;
    }

    float raw = std::sin(twoPi * warpedPhase);

    float driveAmt = 2.0f + (1.0f - pw) * 5.0f;
    float shaped = std::tanh(raw * driveAmt);

    shaped += 0.25f * std::sin(twoPi * phase * 2.0f);
    shaped += 0.12f * std::sin(twoPi * phase * 3.0f + 0.3f);

    return shaped * 0.55f;
}

float BagpipeSynth::droneReed(float phase, float pw) {
    constexpr float twoPi = 2.0f * static_cast<float>(M_PI);

    float warpedPhase;
    if (phase < pw) {
        warpedPhase = phase / pw * 0.5f;
    } else {
        warpedPhase = 0.5f + (phase - pw) / (1.0f - pw) * 0.5f;
    }

    float raw = std::sin(twoPi * warpedPhase);

    float drive = 1.5f + (1.0f - pw) * 2.5f;
    float shaped = std::tanh(raw * drive);

    shaped += 0.15f * std::sin(twoPi * phase * 2.0f);

    return shaped * 0.5f;
}

float BagpipeSynth::renderDrone(DroneState& ds, float freq, float pw, float brightness, float noiseAmt, float pressure) {
    ds.driftPhase += 0.17f / static_cast<float>(m_sr);
    if (ds.driftPhase >= 1.0f) {
        ds.driftPhase -= 1.0f;
    }

    float drift = std::sin(2.0f * static_cast<float>(M_PI) * ds.driftPhase) * 0.15f;
    drift += std::sin(2.0f * static_cast<float>(M_PI) * ds.driftPhase * 2.7f) * 0.08f;
    float driftedFreq = freq * (1.0f + drift / 1200.0f);

    double phaseInc = static_cast<double>(driftedFreq) / m_sr;
    float p = static_cast<float>(ds.phase);
    float reed = droneReed(p, pw);

    ds.phase += phaseInc;
    if (ds.phase >= 1.0) {
        ds.phase -= 1.0;
    }

    if (noiseAmt > 0.001f) {
        reed += cheapNoise() * noiseAmt * 0.3f;
    }

    float droneRes = svfBandpass(reed, driftedFreq, 8.0f, ds.ic1, ds.ic2);
    float sig = reed * 0.4f + droneRes * 0.6f;

    float lpAlpha = calcLpAlpha(brightness);
    ds.lp1 += lpAlpha * (sig - ds.lp1);
    sig = ds.lp1;

    float lpAlpha2 = calcLpAlpha(brightness * 1.2f);
    ds.lp2 += lpAlpha2 * (sig - ds.lp2);
    sig = ds.lp2;

    return sig * pressure;
}

float BagpipeSynth::applyBodyEQ(float input, float lowDb, float midDb, float highDb) {
    float loGain = std::pow(10.0f, lowDb / 20.0f);
    float loAlpha = calcLpAlpha(200.0f);
    m_bodyLo_z1 += loAlpha * (input - m_bodyLo_z1);
    float loComponent = m_bodyLo_z1;
    float hiComponent = input - m_bodyLo_z1;

    float midGain = std::pow(10.0f, midDb / 20.0f) - 1.0f;
    float midBp = svfBandpass(input, 1200.0f, 2.5f, m_bodyMid_ic1, m_bodyMid_ic2);

    float hiGain = std::pow(10.0f, highDb / 20.0f);
    float hiAlpha = calcLpAlpha(4000.0f);
    m_bodyHi_z1 += hiAlpha * (hiComponent - m_bodyHi_z1);
    float hiLow = m_bodyHi_z1;
    float hiHigh = hiComponent - m_bodyHi_z1;

    return loComponent * loGain + hiLow + hiHigh * hiGain + midBp * midGain;
}

void BagpipeSynth::injectGraceNote(int voiceIdx, int graceNote, int durationSamples) {
    if (voiceIdx < 0 || voiceIdx >= static_cast<int>(m_chanterState.size())) {
        return;
    }

    auto& cs = m_chanterState[voiceIdx];
    int tonicNote = static_cast<int>(paramSmoothed(pTonicNote) + 0.5f);

    if (durationSamples <= 0) {
        durationSamples = static_cast<int>(m_sr * 0.025);
    }

    cs.graceActive = true;
    cs.graceFreq = bagpipeFreq(graceNote, tonicNote);
    cs.graceSamplesLeft = durationSamples;
}

void BagpipeSynth::injectCut(int voiceIdx) {
    if (voiceIdx < 0 || voiceIdx >= static_cast<int>(m_chanterState.size())) {
        return;
    }

    int tonicNote = static_cast<int>(paramSmoothed(pTonicNote) + 0.5f);
    int highA = tonicNote + 12;
    injectGraceNote(voiceIdx, highA, static_cast<int>(m_sr * 0.015));
}

void BagpipeSynth::injectStrike(int voiceIdx) {
    if (voiceIdx < 0 || voiceIdx >= static_cast<int>(m_chanterState.size())) {
        return;
    }

    int tonicNote = static_cast<int>(paramSmoothed(pTonicNote) + 0.5f);
    int lowG = tonicNote - 2;
    injectGraceNote(voiceIdx, lowG, static_cast<int>(m_sr * 0.018));
}

float BagpipeSynth::calcLpAlpha(float cutoffHz) const {
    cutoffHz = std::clamp(cutoffHz, 20.0f, static_cast<float>(m_sr * 0.49));
    return 1.0f - std::exp(-2.0f * static_cast<float>(M_PI) * cutoffHz / static_cast<float>(m_sr));
}

float BagpipeSynth::svfBandpass(float input, float cutoffHz, float Q, float& ic1eq, float& ic2eq) const {
    cutoffHz = std::clamp(cutoffHz, 20.0f, static_cast<float>(m_sr * 0.49));
    float g = std::tan(static_cast<float>(M_PI) * cutoffHz / static_cast<float>(m_sr));
    float k = 1.0f / std::max(Q, 0.1f);

    float a1 = 1.0f / (1.0f + g * (g + k));
    float a2 = g * a1;
    float a3 = g * a2;

    float v3 = input - ic2eq;
    float v1 = a1 * ic1eq + a2 * v3;
    float v2 = ic2eq + a2 * ic1eq + a3 * v3;

    ic1eq = 2.0f * v1 - ic1eq;
    ic2eq = 2.0f * v2 - ic2eq;

    return v1;
}

float BagpipeSynth::cheapNoise() {
    m_noiseState ^= m_noiseState << 13;
    m_noiseState ^= m_noiseState >> 17;
    m_noiseState ^= m_noiseState << 5;
    return static_cast<float>(m_noiseState) / static_cast<float>(0xFFFFFFFFu) * 2.0f - 1.0f;
}

float BagpipeSynth::cheapNoiseSlow() {
    m_noiseStateSlow ^= m_noiseStateSlow << 13;
    m_noiseStateSlow ^= m_noiseStateSlow >> 17;
    m_noiseStateSlow ^= m_noiseStateSlow << 5;
    return static_cast<float>(m_noiseStateSlow) / static_cast<float>(0xFFFFFFFFu) * 2.0f - 1.0f;
}