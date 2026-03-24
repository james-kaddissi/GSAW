#pragma once

#include <string>

extern const char* const NOTE_NAMES[12];

int freqToMidi(float f);
float midiToFreq(int m);
std::string freqToPitch(float hz);