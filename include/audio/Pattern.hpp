// Pattern.hpp
#pragma once

#include <audio/core/AudioTypes.hpp>

#include <cstdint>
#include <string>
#include <vector>

using namespace gs::audio;

using PatternId = uint64_t;
static constexpr PatternId kInvalidPatternId = 0;
static constexpr int kDefaultPPQN = 96;

struct PatternNote {
    int tick = 0;
    int duration = 96;
    int note = 60;
    int velocity = 100;
    int channel = 0;
};

class Pattern {
public:
    Pattern() = default;
    Pattern(PatternId id, const std::string& name, int lengthTicks = kDefaultPPQN * 16);

    PatternId getId() const;
    const std::string& getName() const;
    void setName(const std::string& n);

    int getLengthTicks() const;
    void setLengthTicks(int t);

    int getLengthBars() const;
    void setLengthBars(int bars);

    void addNote(int tick, int note, int velocity = 100, int duration = 96, int channel = 0);
    void addNote(const PatternNote& n);
    void removeNote(int index);
    void clearNotes();

    const std::vector<PatternNote>& getNotes() const;
    int getNoteCount() const;

    void sort();

    void renderToMidi(MidiBuffer& out, double tickStart, double tickEnd, double bpm, double sampleRate, int blockFrames) const;

    bool isEmpty() const;

private:
    PatternId m_id = kInvalidPatternId;
    std::string m_name;
    int m_lengthTicks = kDefaultPPQN * 16;
    std::vector<PatternNote> m_notes;
    bool m_sorted = true;
};