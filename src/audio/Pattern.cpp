#include "audio/Pattern.hpp"

#include <algorithm>

bool PatternRange::contains(int tick) const {
    return tick >= startTick && tick < endTick;
}

Pattern::Pattern() = default;

Pattern::Pattern(PatternId id, std::string name, int lengthTicks)
    : m_id(id),
      m_name(std::move(name)),
      m_lengthTicks(std::max(1, lengthTicks)) {
}

PatternId Pattern::getId() const {
    return m_id;
}

const std::string& Pattern::getName() const {
    return m_name;
}

void Pattern::setName(const std::string& name) {
    m_name = name;
}

int Pattern::getLengthTicks() const {
    return m_lengthTicks;
}

void Pattern::setLengthTicks(int ticks) {
    m_lengthTicks = std::max(1, ticks);
}

int Pattern::getPpqn() const {
    return m_ppqn;
}

void Pattern::setPpqn(int ppqn) {
    m_ppqn = std::max(1, ppqn);
}

int Pattern::getLengthBars(int numerator) const {
    const int ticksPerBar = m_ppqn * numerator;
    return ticksPerBar > 0 ? (m_lengthTicks / ticksPerBar) : 0;
}

void Pattern::setLengthBars(int bars, int numerator) {
    m_lengthTicks = std::max(1, bars * m_ppqn * numerator);
}

bool Pattern::isMuted() const {
    return m_muted;
}

void Pattern::setMuted(bool muted) {
    m_muted = muted;
}

bool Pattern::isLooping() const {
    return m_looping;
}

void Pattern::setLooping(bool looping) {
    m_looping = looping;
}

bool Pattern::isEmpty() const {
    return m_notes.empty() &&
           m_automation.empty() &&
           m_pitchBends.empty() &&
           m_aftertouch.empty() &&
           m_programChanges.empty() &&
           m_stepTriggers.empty();
}

NoteId Pattern::addNote(PatternNote note) {
    note.id = nextNoteId();
    clampNote(note);
    m_notes.push_back(note);
    m_notesDirty = true;
    return note.id;
}

NoteId Pattern::addNote(
    RackChannelId targetChannelId,
    int tick,
    int note,
    float velocity,
    int duration,
    float pan,
    float release,
    float finePitch,
    float probability
) {
    PatternNote n;
    n.targetChannelId = targetChannelId;
    n.tick = tick;
    n.duration = duration;
    n.note = note;
    n.velocity = velocity;
    n.pan = pan;
    n.release = release;
    n.finePitch = finePitch;
    n.probability = probability;
    return addNote(n);
}

bool Pattern::removeNote(NoteId id) {
    return eraseById(m_notes, id);
}

PatternNote* Pattern::findNote(NoteId id) {
    return findById(m_notes, id);
}

const PatternNote* Pattern::findNote(NoteId id) const {
    return findById(m_notes, id);
}

bool Pattern::updateNote(NoteId id, const PatternNote& updated) {
    if (auto* n = findNote(id)) {
        PatternNote copy = updated;
        copy.id = id;
        clampNote(copy);
        *n = copy;
        m_notesDirty = true;
        return true;
    }
    return false;
}

void Pattern::clearNotes() {
    m_notes.clear();
    m_notesDirty = false;
}

EventId Pattern::addAutomationPoint(AutomationPoint point) {
    point.id = nextEventId();
    point.tick = std::max(0, point.tick);
    m_automation.push_back(point);
    m_automationDirty = true;
    return point.id;
}

bool Pattern::removeAutomationPoint(EventId id) {
    return eraseById(m_automation, id);
}

AutomationPoint* Pattern::findAutomationPoint(EventId id) {
    return findById(m_automation, id);
}

const AutomationPoint* Pattern::findAutomationPoint(EventId id) const {
    return findById(m_automation, id);
}

void Pattern::clearAutomation() {
    m_automation.clear();
    m_automationDirty = false;
}

EventId Pattern::addPitchBend(PitchBendEvent event) {
    event.id = nextEventId();
    event.tick = std::max(0, event.tick);
    event.value = std::clamp(event.value, -1.0f, 1.0f);
    m_pitchBends.push_back(event);
    m_pitchBendsDirty = true;
    return event.id;
}

bool Pattern::removePitchBend(EventId id) {
    return eraseById(m_pitchBends, id);
}

void Pattern::clearPitchBends() {
    m_pitchBends.clear();
    m_pitchBendsDirty = false;
}

EventId Pattern::addAftertouch(AftertouchEvent event) {
    event.id = nextEventId();
    event.tick = std::max(0, event.tick);
    event.value = std::clamp(event.value, 0.0f, 1.0f);
    m_aftertouch.push_back(event);
    m_aftertouchDirty = true;
    return event.id;
}

bool Pattern::removeAftertouch(EventId id) {
    return eraseById(m_aftertouch, id);
}

void Pattern::clearAftertouch() {
    m_aftertouch.clear();
    m_aftertouchDirty = false;
}

EventId Pattern::addProgramChange(ProgramChangeEvent event) {
    event.id = nextEventId();
    event.tick = std::max(0, event.tick);
    m_programChanges.push_back(event);
    m_programChangesDirty = true;
    return event.id;
}

bool Pattern::removeProgramChange(EventId id) {
    return eraseById(m_programChanges, id);
}

void Pattern::clearProgramChanges() {
    m_programChanges.clear();
    m_programChangesDirty = false;
}

EventId Pattern::addStepTrigger(StepTrigger trigger) {
    trigger.id = nextEventId();
    trigger.tick = std::max(0, trigger.tick);
    trigger.lengthTicks = std::max(1, trigger.lengthTicks);
    trigger.velocity = std::clamp(trigger.velocity, 0.0f, 1.0f);
    trigger.probability = std::clamp(trigger.probability, 0.0f, 1.0f);
    m_stepTriggers.push_back(trigger);
    m_stepTriggersDirty = true;
    return trigger.id;
}

bool Pattern::removeStepTrigger(EventId id) {
    return eraseById(m_stepTriggers, id);
}

void Pattern::clearStepTriggers() {
    m_stepTriggers.clear();
    m_stepTriggersDirty = false;
}

const std::vector<PatternNote>& Pattern::getNotes() const {
    return m_notes;
}

const std::vector<AutomationPoint>& Pattern::getAutomationPoints() const {
    return m_automation;
}

const std::vector<PitchBendEvent>& Pattern::getPitchBends() const {
    return m_pitchBends;
}

const std::vector<AftertouchEvent>& Pattern::getAftertouchEvents() const {
    return m_aftertouch;
}

const std::vector<ProgramChangeEvent>& Pattern::getProgramChanges() const {
    return m_programChanges;
}

const std::vector<StepTrigger>& Pattern::getStepTriggers() const {
    return m_stepTriggers;
}

int Pattern::getNoteCount() const {
    return static_cast<int>(m_notes.size());
}

int Pattern::getAutomationPointCount() const {
    return static_cast<int>(m_automation.size());
}

int Pattern::getPitchBendCount() const {
    return static_cast<int>(m_pitchBends.size());
}

int Pattern::getAftertouchCount() const {
    return static_cast<int>(m_aftertouch.size());
}

int Pattern::getProgramChangeCount() const {
    return static_cast<int>(m_programChanges.size());
}

int Pattern::getStepTriggerCount() const {
    return static_cast<int>(m_stepTriggers.size());
}

void Pattern::sortAll() {
    if (m_notesDirty) {
        std::sort(m_notes.begin(), m_notes.end(), [](const PatternNote& a, const PatternNote& b) {
            if (a.tick != b.tick) return a.tick < b.tick;
            if (a.targetChannelId != b.targetChannelId) return a.targetChannelId < b.targetChannelId;
            if (a.note != b.note) return a.note < b.note;
            return a.id < b.id;
        });
        m_notesDirty = false;
    }

    if (m_automationDirty) {
        std::sort(m_automation.begin(), m_automation.end(), [](const AutomationPoint& a, const AutomationPoint& b) {
            if (a.tick != b.tick) return a.tick < b.tick;
            if (a.targetChannelId != b.targetChannelId) return a.targetChannelId < b.targetChannelId;
            if (a.parameterId != b.parameterId) return a.parameterId < b.parameterId;
            return a.id < b.id;
        });
        m_automationDirty = false;
    }

    if (m_pitchBendsDirty) {
        std::sort(m_pitchBends.begin(), m_pitchBends.end(), [](const PitchBendEvent& a, const PitchBendEvent& b) {
            if (a.tick != b.tick) return a.tick < b.tick;
            if (a.targetChannelId != b.targetChannelId) return a.targetChannelId < b.targetChannelId;
            return a.id < b.id;
        });
        m_pitchBendsDirty = false;
    }

    if (m_aftertouchDirty) {
        std::sort(m_aftertouch.begin(), m_aftertouch.end(), [](const AftertouchEvent& a, const AftertouchEvent& b) {
            if (a.tick != b.tick) return a.tick < b.tick;
            if (a.targetChannelId != b.targetChannelId) return a.targetChannelId < b.targetChannelId;
            if (a.note != b.note) return a.note < b.note;
            return a.id < b.id;
        });
        m_aftertouchDirty = false;
    }

    if (m_programChangesDirty) {
        std::sort(m_programChanges.begin(), m_programChanges.end(), [](const ProgramChangeEvent& a, const ProgramChangeEvent& b) {
            if (a.tick != b.tick) return a.tick < b.tick;
            if (a.targetChannelId != b.targetChannelId) return a.targetChannelId < b.targetChannelId;
            return a.id < b.id;
        });
        m_programChangesDirty = false;
    }

    if (m_stepTriggersDirty) {
        std::sort(m_stepTriggers.begin(), m_stepTriggers.end(), [](const StepTrigger& a, const StepTrigger& b) {
            if (a.tick != b.tick) return a.tick < b.tick;
            if (a.targetChannelId != b.targetChannelId) return a.targetChannelId < b.targetChannelId;
            if (a.lane != b.lane) return a.lane < b.lane;
            return a.id < b.id;
        });
        m_stepTriggersDirty = false;
    }
}

PatternRange Pattern::getRange() const {
    return {0, m_lengthTicks};
}

void Pattern::clampNote(PatternNote& note) {
    note.tick = std::max(0, note.tick);
    note.duration = std::max(1, note.duration);
    note.note = std::clamp(note.note, 0, 127);
    note.velocity = std::clamp(note.velocity, 0.0f, 1.0f);
    note.pan = std::clamp(note.pan, -1.0f, 1.0f);
    note.release = std::max(0.0f, note.release);
    note.finePitch = std::clamp(note.finePitch, -1.0f, 1.0f);
    note.probability = std::clamp(note.probability, 0.0f, 1.0f);
}

NoteId Pattern::nextNoteId() {
    return m_nextNoteId++;
}

EventId Pattern::nextEventId() {
    return m_nextEventId++;
}