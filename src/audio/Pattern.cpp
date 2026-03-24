#include "audio/Pattern.hpp"

#include <algorithm>
#include <cmath>

Pattern::Pattern(PatternId id, const std::string& name, int lengthTicks)
    : m_id(id), m_name(name), m_lengthTicks(lengthTicks) {
}

PatternId Pattern::getId() const {
    return m_id;
}

const std::string& Pattern::getName() const {
    return m_name;
}

void Pattern::setName(const std::string& n) {
    m_name = n;
}

int Pattern::getLengthTicks() const {
    return m_lengthTicks;
}

void Pattern::setLengthTicks(int t) {
    m_lengthTicks = t;
}

int Pattern::getLengthBars() const {
    return m_lengthTicks / (kDefaultPPQN * 4);
}

void Pattern::setLengthBars(int bars) {
    m_lengthTicks = bars * kDefaultPPQN * 4;
}

void Pattern::addNote(int tick, int note, int velocity, int duration, int channel) {
    m_notes.push_back({tick, duration, note, velocity, channel});
    m_sorted = false;
}

void Pattern::addNote(const PatternNote& n) {
    m_notes.push_back(n);
    m_sorted = false;
}

void Pattern::removeNote(int index) {
    if (index >= 0 && index < static_cast<int>(m_notes.size())) {
        m_notes.erase(m_notes.begin() + index);
    }
}

void Pattern::clearNotes() {
    m_notes.clear();
}

const std::vector<PatternNote>& Pattern::getNotes() const {
    return m_notes;
}

int Pattern::getNoteCount() const {
    return static_cast<int>(m_notes.size());
}

void Pattern::sort() {
    if (m_sorted) {
        return;
    }

    std::sort(m_notes.begin(), m_notes.end(), [](const PatternNote& a, const PatternNote& b) {
        return a.tick < b.tick;
    });
    m_sorted = true;
}

void Pattern::renderToMidi(MidiBuffer& out, double tickStart, double tickEnd, double bpm, double sampleRate, int blockFrames) const {
    if (m_notes.empty() || m_lengthTicks <= 0) {
        return;
    }

    double ticksPerSample = (bpm * kDefaultPPQN) / (60.0 * sampleRate);
    double samplesPerTick = 1.0 / ticksPerSample;

    double cursor = tickStart;
    while (cursor < tickEnd) {
        double patternPos = std::fmod(cursor, static_cast<double>(m_lengthTicks));
        if (patternPos < 0) {
            patternPos += m_lengthTicks;
        }

        double remaining = tickEnd - cursor;
        double patternRemaining = m_lengthTicks - patternPos;
        double span = std::min(remaining, patternRemaining);
        double spanEnd = patternPos + span;

        for (const auto& note : m_notes) {
            double noteTick = static_cast<double>(note.tick);
            double noteEnd = noteTick + static_cast<double>(note.duration);

            if (noteTick >= patternPos && noteTick < spanEnd) {
                double sampleExact = (cursor - tickStart + (noteTick - patternPos)) * samplesPerTick;
                int sampleOffset = std::clamp(static_cast<int>(sampleExact), 0, blockFrames - 1);
                out.add(MidiEvent::noteOn(note.channel, note.note, note.velocity, sampleOffset));
            }

            if (noteEnd >= patternPos && noteEnd < spanEnd) {
                double sampleExact = (cursor - tickStart + (noteEnd - patternPos)) * samplesPerTick;
                int sampleOffset = std::clamp(static_cast<int>(sampleExact), 0, blockFrames - 1);
                out.add(MidiEvent::noteOff(note.channel, note.note, sampleOffset));
            }
        }

        cursor += span;
    }
}

bool Pattern::isEmpty() const {
    return m_notes.empty();
}