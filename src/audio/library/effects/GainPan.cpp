#include "audio/library/effects/GainPan.hpp"

#include <cmath>

#define M_PI 3.141592653589793238462643383279502884

GainPan::GainPan() {
    addParam({"gain_db","Gain (dB)",-60.0f,12.0f,0.0f});
    addParam({"pan","Pan",-1.0f,1.0f,0.0f});
}

const char* GainPan::name() const {
    return "Gain/Pan";
}

void GainPan::prepare(const gs::audio::ProcessContext& ctx) {
    initSmoothing(ctx.sampleRate,5.0f);
}

void GainPan::reset() {
}

void GainPan::process(gs::audio::AudioBuffer& io,const gs::audio::ProcessContext&) {
    float gDb = paramSmoothed(0);
    float pan = paramSmoothed(1);

    float g = std::pow(10.0f,gDb / 20.0f);

    float angle = (pan * 0.5f + 0.5f) * float(M_PI) * 0.5f;
    float lGain = std::cos(angle) * g;
    float rGain = std::sin(angle) * g;

    float* L = io.channel(0);
    float* R = io.channel(1);

    for (int i = 0; i < io.frames; ++i) {
        L[i] *= lGain;
        R[i] *= rGain;
    }
}