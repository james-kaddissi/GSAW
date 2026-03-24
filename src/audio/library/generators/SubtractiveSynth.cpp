#include "audio/library/generators/SubtractiveSynth.hpp"

#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>
#include <cstring>

#ifndef M_PI
#define M_PI 3.141592653589793238462643383279502884
#endif

SubSynth::SubSynth(int voices)
    : GeneratorBase(voices) {
    addParam({"waveform", "Waveform", 0.0f, 3.0f, 1.0f, 1.0f});
    addParam({"attack", "Attack (ms)", 1.0f, 5000.0f, 10.0f});
    addParam({"decay", "Decay (ms)", 1.0f, 5000.0f, 200.0f});
    addParam({"sustain", "Sustain", 0.0f, 1.0f, 0.7f});
    addParam({"release", "Release (ms)", 1.0f, 5000.0f, 300.0f});
    addParam({"cutoff", "Filter Cutoff", 20.0f, 20000.0f, 8000.0f});
    addParam({"resonance", "Resonance", 0.0f, 1.0f, 0.0f});
    addParam({"envmod", "Env → Cutoff", 0.0f, 1.0f, 0.3f});
    addParam({"detune", "Detune (cents)", -100.0f, 100.0f, 0.0f});
    addParam({"gain", "Gain (dB)", -30.0f, 6.0f, -6.0f});
}

const char* SubSynth::name() const {
    return "SubSynth";
}

bool SubSynth::hasTail() const {
    return true;
}

void SubSynth::prepare(const PC& ctx) {
    m_sr = ctx.sampleRate;
    initSmoothing(m_sr, 5.0f);
}

void SubSynth::reset() {
    for (auto& v : m_voices) {
        v.active = false;
        v.releasing = false;
        v.phase = 0.0;
        v.envelope = 0.0f;
        std::memset(v.userData, 0, sizeof(v.userData));
    }
}

void SubSynth::onNoteOn(Voice& v, const PC&) {
    float freq = midiNoteToHz(v.note);
    v.userData[0] = 0.0f;
    v.userData[1] = 0.0f;
    v.userData[2] = 0.0f;
    v.userData[3] = 0.0f;
    v.userData[4] = freq;
    v.userData[5] = static_cast<float>(v.velocity) / 127.0f;
    v.phase = 0.0;
    v.releasing = false;
}

void SubSynth::onNoteOff(Voice& v, const PC&) {
    v.releasing = true;
    v.userData[2] = 3.0f;
}

float SubSynth::renderVoice(Voice& v, const PC&) {
    float waveformIdx = m_params[0].get();
    float cutoff = paramSmoothed(5);
    float resonance = paramSmoothed(6);
    float envMod = paramSmoothed(7);
    float detuneCents = paramSmoothed(8);
    float gainDb = paramSmoothed(9);

    float gainLin = std::pow(10.0f, gainDb / 20.0f);

    float freq = v.userData[4];
    if (std::abs(detuneCents) > 0.01f) {
        freq *= std::pow(2.0f, detuneCents / 1200.0f);
    }

    double phaseInc = freq / m_sr;
    float sample = oscillator(v.phase, static_cast<int>(waveformIdx + 0.5f));
    v.phase += phaseInc;
    if (v.phase >= 1.0) {
        v.phase -= 1.0;
    }

    float env = v.userData[3];

    float modCutoff = cutoff + envMod * env * (20000.0f - cutoff);
    modCutoff = std::clamp(modCutoff, 20.0f, 20000.0f);

    float filtered = svfLowpass(sample, modCutoff, resonance, v.userData[0], v.userData[1]);

    return filtered * env * v.userData[5] * gainLin;
}

bool SubSynth::advanceVoice(Voice& v, const PC&) {
    float attack = m_params[1].get();
    float decay = m_params[2].get();
    float sustain = m_params[3].get();
    float release = m_params[4].get();

    int stage = static_cast<int>(v.userData[2]);
    float level = v.userData[3];

    switch (stage) {
        case 0: {
            float rate = 1.0f / std::max(1.0f, attack * 0.001f * static_cast<float>(m_sr));
            level += rate;
            if (level >= 1.0f) {
                level = 1.0f;
                v.userData[2] = 1.0f;
            }
            break;
        }
        case 1: {
            float rate = 1.0f / std::max(1.0f, decay * 0.001f * static_cast<float>(m_sr));
            level -= rate * (level - sustain);
            if (std::abs(level - sustain) < 0.001f) {
                level = sustain;
                v.userData[2] = 2.0f;
            }
            break;
        }
        case 2:
            level = sustain;
            break;
        case 3: {
            float rate = 1.0f / std::max(1.0f, release * 0.001f * static_cast<float>(m_sr));
            level -= rate * level;
            if (level < 0.0001f) {
                level = 0.0f;
                v.userData[2] = 4.0f;
                v.userData[3] = 0.0f;
                return false;
            }
            break;
        }
        default:
            return false;
    }

    v.userData[3] = level;
    return true;
}

float SubSynth::midiNoteToHz(int note) {
    return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
}

float SubSynth::oscillator(double phase, int waveform) {
    float p = static_cast<float>(phase);
    switch (waveform) {
        case 0:
            return std::sin(2.0f * static_cast<float>(M_PI) * p);
        case 1:
            return sawBLEP(p);
        case 2:
            return squareBLEP(p);
        case 3:
            return 4.0f * std::abs(p - 0.5f) - 1.0f;
        default:
            return std::sin(2.0f * static_cast<float>(M_PI) * p);
    }
}

float SubSynth::polyBLEP(float t, float dt) {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

float SubSynth::sawBLEP(float phase) {
    float val = 2.0f * phase - 1.0f;
    val -= polyBLEP(phase);
    return val;
}

float SubSynth::squareBLEP(float phase) {
    float val = phase < 0.5f ? 1.0f : -1.0f;
    val += polyBLEP(phase);
    val -= polyBLEP(std::fmod(phase + 0.5f, 1.0f));
    return val;
}

float SubSynth::svfLowpass(float input, float cutoffHz, float res, float& ic1eq, float& ic2eq) const {
    float g = std::tan(static_cast<float>(M_PI) * cutoffHz / static_cast<float>(m_sr));
    float k = 2.0f - 2.0f * res;
    k = std::max(k, 0.01f);

    float a1 = 1.0f / (1.0f + g * (g + k));
    float a2 = g * a1;
    float a3 = g * a2;

    float v3 = input - ic2eq;
    float v1 = a1 * ic1eq + a2 * v3;
    float v2 = ic2eq + a2 * ic1eq + a3 * v3;

    ic1eq = 2.0f * v1 - ic1eq;
    ic2eq = 2.0f * v2 - ic2eq;

    return v2;
}