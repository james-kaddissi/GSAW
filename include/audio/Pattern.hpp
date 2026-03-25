#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <algorithm>
#include <utility>

using PatternId = uint64_t;
using NoteId = uint64_t;
using EventId = uint64_t;
using RackChannelId = uint64_t;
using ParameterId = uint64_t;

static constexpr PatternId kInvalidPatternId = 0;
static constexpr NoteId kInvalidNoteId = 0;
static constexpr EventId kInvalidEventId = 0;
static constexpr RackChannelId kInvalidRackChannelId = 0;
static constexpr int kDefaultPPQN = 96;

//temporary architecture. todo:
// move out render/playback code + midi emission code + automation evaluation code
// Pattern should only handle data and data emission

// add PatternClip type as layer between Pattern and Arrangement type in playlist

enum class AutomationInterpolation {
    Step,
    Linear
};

enum class StepTriggerMode {
    Note,
    Gate,
    Retrigger
};

struct PatternNote {
    NoteId id = kInvalidNoteId;
    RackChannelId targetChannelId = kInvalidRackChannelId;

    int tick = 0;
    int duration = kDefaultPPQN;
    int note = 60;
    float velocity = 1.0f;

    float pan = 0.0f;
    float release = 0.0f;
    float finePitch = 0.0f;
    float probability = 1.0f;

    bool muted = false;
    bool selected = false;
};

struct AutomationPoint {
    EventId id = kInvalidEventId;
    RackChannelId targetChannelId = kInvalidRackChannelId;
    ParameterId parameterId = 0;

    int tick = 0;
    float value = 0.0f;
    AutomationInterpolation interpolation = AutomationInterpolation::Linear;

    bool muted = false;
    bool selected = false;
};

struct PitchBendEvent {
    EventId id = kInvalidEventId;
    RackChannelId targetChannelId = kInvalidRackChannelId;

    int tick = 0;
    float value = 0.0f;

    bool muted = false;
    bool selected = false;
};

struct AftertouchEvent {
    EventId id = kInvalidEventId;
    RackChannelId targetChannelId = kInvalidRackChannelId;

    int tick = 0;
    float value = 0.0f;
    int note = -1;

    bool muted = false;
    bool selected = false;
};

struct ProgramChangeEvent {
    EventId id = kInvalidEventId;
    RackChannelId targetChannelId = kInvalidRackChannelId;

    int tick = 0;
    int program = 0;
    int bankMsb = -1;
    int bankLsb = -1;

    bool muted = false;
    bool selected = false;
};

struct StepTrigger {
    EventId id = kInvalidEventId;
    RackChannelId targetChannelId = kInvalidRackChannelId;

    int tick = 0;
    int lane = 0;
    int note = 60;
    float velocity = 1.0f;
    int lengthTicks = kDefaultPPQN / 4;
    StepTriggerMode mode = StepTriggerMode::Note;
    float probability = 1.0f;

    bool muted = false;
    bool selected = false;
};

struct PatternRange {
    int startTick = 0;
    int endTick = 0;

    bool contains(int tick) const {
        return tick >= startTick && tick < endTick;
    }
};

class Pattern {
public:
    Pattern() = default;
    Pattern(PatternId id, std::string name, int lengthTicks = kDefaultPPQN * 16)
        : m_id(id), m_name(std::move(name)), m_lengthTicks(lengthTicks) {}

    PatternId getId() const { return m_id; }

    const std::string& getName() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    int getLengthTicks() const { return m_lengthTicks; }
    void setLengthTicks(int ticks) { m_lengthTicks = std::max(1, ticks); }

    int getPpqn() const { return m_ppqn; }
    void setPpqn(int ppqn) { m_ppqn = std::max(1, ppqn); }

    int getLengthBars(int numerator = 4) const {
        const int ticksPerBar = m_ppqn * numerator;
        return ticksPerBar > 0 ? (m_lengthTicks / ticksPerBar) : 0;
    }

    void setLengthBars(int bars, int numerator = 4) {
        m_lengthTicks = std::max(1, bars * m_ppqn * numerator);
    }

    bool isMuted() const { return m_muted; }
    void setMuted(bool muted) { m_muted = muted; }

    bool isLooping() const { return m_looping; }
    void setLooping(bool looping) { m_looping = looping; }

    bool isEmpty() const {
        return m_notes.empty() &&
               m_automation.empty() &&
               m_pitchBends.empty() &&
               m_aftertouch.empty() &&
               m_programChanges.empty() &&
               m_stepTriggers.empty();
    }

    NoteId addNote(PatternNote note) {
        note.id = nextNoteId();
        clampNote(note);
        m_notes.push_back(note);
        m_notesDirty = true;
        return note.id;
    }

    NoteId addNote(
        RackChannelId targetChannelId,
        int tick,
        int note,
        float velocity = 1.0f,
        int duration = kDefaultPPQN,
        float pan = 0.0f,
        float release = 0.0f,
        float finePitch = 0.0f,
        float probability = 1.0f
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

    bool removeNote(NoteId id) {
        return eraseById(m_notes, id);
    }

    PatternNote* findNote(NoteId id) {
        return findById(m_notes, id);
    }

    const PatternNote* findNote(NoteId id) const {
        return findById(m_notes, id);
    }

    bool updateNote(NoteId id, const PatternNote& updated) {
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

    void clearNotes() {
        m_notes.clear();
        m_notesDirty = false;
    }

    EventId addAutomationPoint(AutomationPoint point) {
        point.id = nextEventId();
        point.tick = std::max(0, point.tick);
        m_automation.push_back(point);
        m_automationDirty = true;
        return point.id;
    }

    bool removeAutomationPoint(EventId id) {
        return eraseById(m_automation, id);
    }

    AutomationPoint* findAutomationPoint(EventId id) {
        return findById(m_automation, id);
    }

    const AutomationPoint* findAutomationPoint(EventId id) const {
        return findById(m_automation, id);
    }

    void clearAutomation() {
        m_automation.clear();
        m_automationDirty = false;
    }

    EventId addPitchBend(PitchBendEvent event) {
        event.id = nextEventId();
        event.tick = std::max(0, event.tick);
        event.value = std::clamp(event.value, -1.0f, 1.0f);
        m_pitchBends.push_back(event);
        m_pitchBendsDirty = true;
        return event.id;
    }

    bool removePitchBend(EventId id) {
        return eraseById(m_pitchBends, id);
    }

    void clearPitchBends() {
        m_pitchBends.clear();
        m_pitchBendsDirty = false;
    }

    EventId addAftertouch(AftertouchEvent event) {
        event.id = nextEventId();
        event.tick = std::max(0, event.tick);
        event.value = std::clamp(event.value, 0.0f, 1.0f);
        m_aftertouch.push_back(event);
        m_aftertouchDirty = true;
        return event.id;
    }

    bool removeAftertouch(EventId id) {
        return eraseById(m_aftertouch, id);
    }

    void clearAftertouch() {
        m_aftertouch.clear();
        m_aftertouchDirty = false;
    }

    EventId addProgramChange(ProgramChangeEvent event) {
        event.id = nextEventId();
        event.tick = std::max(0, event.tick);
        m_programChanges.push_back(event);
        m_programChangesDirty = true;
        return event.id;
    }

    bool removeProgramChange(EventId id) {
        return eraseById(m_programChanges, id);
    }

    void clearProgramChanges() {
        m_programChanges.clear();
        m_programChangesDirty = false;
    }

    EventId addStepTrigger(StepTrigger trigger) {
        trigger.id = nextEventId();
        trigger.tick = std::max(0, trigger.tick);
        trigger.lengthTicks = std::max(1, trigger.lengthTicks);
        trigger.velocity = std::clamp(trigger.velocity, 0.0f, 1.0f);
        trigger.probability = std::clamp(trigger.probability, 0.0f, 1.0f);
        m_stepTriggers.push_back(trigger);
        m_stepTriggersDirty = true;
        return trigger.id;
    }

    bool removeStepTrigger(EventId id) {
        return eraseById(m_stepTriggers, id);
    }

    void clearStepTriggers() {
        m_stepTriggers.clear();
        m_stepTriggersDirty = false;
    }

    const std::vector<PatternNote>& getNotes() const { return m_notes; }
    const std::vector<AutomationPoint>& getAutomationPoints() const { return m_automation; }
    const std::vector<PitchBendEvent>& getPitchBends() const { return m_pitchBends; }
    const std::vector<AftertouchEvent>& getAftertouchEvents() const { return m_aftertouch; }
    const std::vector<ProgramChangeEvent>& getProgramChanges() const { return m_programChanges; }
    const std::vector<StepTrigger>& getStepTriggers() const { return m_stepTriggers; }

    int getNoteCount() const { return static_cast<int>(m_notes.size()); }
    int getAutomationPointCount() const { return static_cast<int>(m_automation.size()); }
    int getPitchBendCount() const { return static_cast<int>(m_pitchBends.size()); }
    int getAftertouchCount() const { return static_cast<int>(m_aftertouch.size()); }
    int getProgramChangeCount() const { return static_cast<int>(m_programChanges.size()); }
    int getStepTriggerCount() const { return static_cast<int>(m_stepTriggers.size()); }

    void sortAll() {
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

    PatternRange getRange() const {
        return {0, m_lengthTicks};
    }

private:
    static void clampNote(PatternNote& note) {
        note.tick = std::max(0, note.tick);
        note.duration = std::max(1, note.duration);
        note.note = std::clamp(note.note, 0, 127);
        note.velocity = std::clamp(note.velocity, 0.0f, 1.0f);
        note.pan = std::clamp(note.pan, -1.0f, 1.0f);
        note.release = std::max(0.0f, note.release);
        note.finePitch = std::clamp(note.finePitch, -1.0f, 1.0f);
        note.probability = std::clamp(note.probability, 0.0f, 1.0f);
    }

    template <typename T>
    static T* findById(std::vector<T>& items, uint64_t id) {
        auto it = std::find_if(items.begin(), items.end(), [id](const T& item) {
            return item.id == id;
        });
        return it != items.end() ? &(*it) : nullptr;
    }

    template <typename T>
    static const T* findById(const std::vector<T>& items, uint64_t id) {
        auto it = std::find_if(items.begin(), items.end(), [id](const T& item) {
            return item.id == id;
        });
        return it != items.end() ? &(*it) : nullptr;
    }

    template <typename T>
    static bool eraseById(std::vector<T>& items, uint64_t id) {
        auto it = std::find_if(items.begin(), items.end(), [id](const T& item) {
            return item.id == id;
        });
        if (it == items.end()) {
            return false;
        }
        items.erase(it);
        return true;
    }

    NoteId nextNoteId() { return m_nextNoteId++; }
    EventId nextEventId() { return m_nextEventId++; }

private:
    PatternId m_id = kInvalidPatternId;
    std::string m_name;
    int m_lengthTicks = kDefaultPPQN * 16;
    int m_ppqn = kDefaultPPQN;
    bool m_muted = false;
    bool m_looping = true;

    std::vector<PatternNote> m_notes;
    std::vector<AutomationPoint> m_automation;
    std::vector<PitchBendEvent> m_pitchBends;
    std::vector<AftertouchEvent> m_aftertouch;
    std::vector<ProgramChangeEvent> m_programChanges;
    std::vector<StepTrigger> m_stepTriggers;

    NoteId m_nextNoteId = 1;
    EventId m_nextEventId = 1;

    bool m_notesDirty = false;
    bool m_automationDirty = false;
    bool m_pitchBendsDirty = false;
    bool m_aftertouchDirty = false;
    bool m_programChangesDirty = false;
    bool m_stepTriggersDirty = false;
};