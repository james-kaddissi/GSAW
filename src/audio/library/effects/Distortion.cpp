#include "audio/library/effects/Distortion.hpp"

#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.141592653589793238462643383279502884
#endif

Distortion::Distortion() {
    addParam({"drive", "Drive", 1.0f, 100.0f, 8.0f});
    addParam({"tone", "Tone", 0.0f, 1.0f, 0.5f});
    addParam({"output", "Output (dB)", -30.0f, 6.0f, -6.0f});
    addParam({"bias", "Bias", -1.0f, 1.0f, 0.0f});
    addParam({"tight", "Tight", 0.0f, 1.0f, 0.5f});
}

const char* Distortion::name() const {
    return "Distortion";
}

void Distortion::prepare(const gs::audio::ProcessContext& ctx) {
    initSmoothing(ctx.sampleRate, 5.0f);
    m_sr = ctx.sampleRate;
    m_osRate = m_sr * kOversample;
    resetFilters();
}

void Distortion::reset() {
    resetFilters();
}

void Distortion::process(gs::audio::AudioBuffer& io, const gs::audio::ProcessContext&) {
    float drive = paramSmoothed(0);
    float tone = paramSmoothed(1);
    float outDb = paramSmoothed(2);
    float bias = paramSmoothed(3) * 0.3f;
    float tight = paramSmoothed(4);

    float outGain = std::pow(10.0f, outDb / 20.0f);

    float hpFreq = 20.0f + tight * 380.0f;
    updateHP(m_preHP, hpFreq, m_sr);

    float lpFreq = 800.0f * std::pow(15.0f, tone);
    updateLP(m_postLP, lpFreq, m_osRate);

    float presFreq = 1500.0f;
    float presGain = 2.0f + tone * 4.0f;
    updatePeak(m_presence, presFreq, presGain, 1.2f, m_sr);

    updateLP(m_aaFilter, m_sr * 0.48, m_osRate);

    float* L = io.channel(0);
    float* R = io.channel(1);

    for (int i = 0; i < io.frames; ++i) {
        float sL = biquad(m_preHP[0], L[i]);
        float sR = biquad(m_preHP[1], R[i]);

        float outL = processOversampled(sL, bias, drive, 0);
        float outR = processOversampled(sR, bias, drive, 1);

        outL = biquad(m_presence[0], outL);
        outR = biquad(m_presence[1], outR);

        L[i] = outL * outGain;
        R[i] = outR * outGain;
    }
}

float Distortion::tubeClip(float x, float bias) {
    x += bias;
    float y;
    if (x > 0.0f) {
        y = std::tanh(x);
    } else {
        y = std::tanh(x * 1.2f) / 1.2f;
    }
    return y - std::tanh(bias);
}

float Distortion::hardClip(float x) {
    if (x > 1.0f) return 2.0f / 3.0f;
    if (x < -1.0f) return -2.0f / 3.0f;
    return x - (x * x * x) / 3.0f;
}

float Distortion::processOversampled(float in, float bias, float drive, int ch) {
    float acc = 0.0f;

    for (int os = 0; os < kOversample; ++os) {
        float x = (os == 0) ? in * float(kOversample) : 0.0f;

        x = biquad(m_interpFilter[ch], x);
        x = tubeClip(x * drive * 0.5f, bias);
        x = biquad(m_interstage[ch], x);
        x = hardClip(x * 1.5f);
        x = biquad(m_postLP[ch], x);
        x = biquad(m_aaFilter[ch], x);

        if (os == kOversample - 1) {
            acc = x;
        }
    }

    return acc;
}

float Distortion::biquad(Biquad& f, float in) {
    float out = f.b0 * in + f.z1;
    f.z1 = f.b1 * in - f.a1 * out + f.z2;
    f.z2 = f.b2 * in - f.a2 * out;
    return out;
}

void Distortion::updateLP(Biquad bq[2], double freq, double sr) {
    double w0 = 2.0 * M_PI * freq / sr;
    double cosW = std::cos(w0);
    double alpha = std::sin(w0) / (2.0 * 0.707);
    double a0 = 1.0 + alpha;
    double c = (1.0 - cosW) * 0.5;

    float cb0 = float(c / a0);
    float cb1 = float((1.0 - cosW) / a0);
    float cb2 = float(c / a0);
    float ca1 = float((-2.0 * cosW) / a0);
    float ca2 = float((1.0 - alpha) / a0);

    for (int ch = 0; ch < 2; ++ch) {
        bq[ch].b0 = cb0;
        bq[ch].b1 = cb1;
        bq[ch].b2 = cb2;
        bq[ch].a1 = ca1;
        bq[ch].a2 = ca2;
    }
}

void Distortion::updateHP(Biquad bq[2], double freq, double sr) {
    double w0 = 2.0 * M_PI * freq / sr;
    double cosW = std::cos(w0);
    double alpha = std::sin(w0) / (2.0 * 0.707);
    double a0 = 1.0 + alpha;
    double c = (1.0 + cosW) * 0.5;

    float cb0 = float(c / a0);
    float cb1 = float(-(1.0 + cosW) / a0);
    float cb2 = float(c / a0);
    float ca1 = float((-2.0 * cosW) / a0);
    float ca2 = float((1.0 - alpha) / a0);

    for (int ch = 0; ch < 2; ++ch) {
        bq[ch].b0 = cb0;
        bq[ch].b1 = cb1;
        bq[ch].b2 = cb2;
        bq[ch].a1 = ca1;
        bq[ch].a2 = ca2;
    }
}

void Distortion::updatePeak(Biquad bq[2], double freq, double gainDb, double q, double sr) {
    double A = std::pow(10.0, gainDb / 40.0);
    double w0 = 2.0 * M_PI * freq / sr;
    double sinW = std::sin(w0);
    double cosW = std::cos(w0);
    double alpha = sinW / (2.0 * q);
    double a0 = 1.0 + alpha / A;

    float cb0 = float((1.0 + alpha * A) / a0);
    float cb1 = float((-2.0 * cosW) / a0);
    float cb2 = float((1.0 - alpha * A) / a0);
    float ca1 = float((-2.0 * cosW) / a0);
    float ca2 = float((1.0 - alpha / A) / a0);

    for (int ch = 0; ch < 2; ++ch) {
        bq[ch].b0 = cb0;
        bq[ch].b1 = cb1;
        bq[ch].b2 = cb2;
        bq[ch].a1 = ca1;
        bq[ch].a2 = ca2;
    }
}

void Distortion::resetFilters() {
    auto clear = [](Biquad bq[2]) {
        for (int c = 0; c < 2; ++c) {
            bq[c] = {};
        }
    };

    clear(m_preHP);
    clear(m_postLP);
    clear(m_presence);
    clear(m_aaFilter);
    clear(m_interpFilter);
    clear(m_interstage);

    if (m_osRate > 0) {
        updateLP(m_interstage, 5000.0, m_osRate);
    }

    if (m_osRate > 0) {
        updateLP(m_interpFilter, m_sr * 0.45, m_osRate);
    }
}