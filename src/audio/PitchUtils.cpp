#include "audio/PitchUtils.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

const char* const NOTE_NAMES[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

int freqToMidi(float f) {
    return (f > 0.0f) ? static_cast<int>(std::lround(69.0 + 12.0 * std::log2(f / 440.0f))) : -1;
}

float midiToFreq(int m) {
    return 440.0f * std::pow(2.0f, (m - 69) / 12.0f);
}

std::string freqToPitch(float hz) {
    if (hz <= 0.0f) {
        return "--";
    }

    float midiFloat = 69.0f + 12.0f * std::log2(hz / 440.0f);
    int midi = static_cast<int>(std::lround(midiFloat));

    int noteIndex = (midi % 12 + 12) % 12;
    int octave = (midi / 12) - 1;
    float cents = 100.0f * (midiFloat - static_cast<float>(midi));

    std::ostringstream ss;
    ss << NOTE_NAMES[noteIndex]
       << octave
       << " ("
       << std::showpos
       << std::fixed
       << std::setprecision(1)
       << cents
       << "c)";

    return ss.str();
}