#pragma once

#include "audio/Ids.hpp"

#include <string>
#include <vector>

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

    bool contains(int tick) const;
};

class Pattern {
public:
    Pattern();
    Pattern(PatternId id, std::string name, int lengthTicks = kDefaultPPQN * 16);

    PatternId getId() const;

    const std::string& getName() const;
    void setName(const std::string& name);

    int getLengthTicks() const;
    void setLengthTicks(int ticks);

    int getPpqn() const;
    void setPpqn(int ppqn);

    int getLengthBars(int numerator = 4) const;
    void setLengthBars(int bars, int numerator = 4);

    bool isMuted() const;
    void setMuted(bool muted);

    bool isLooping() const;
    void setLooping(bool looping);

    bool isEmpty() const;

    NoteId addNote(PatternNote note);
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
    );

    bool removeNote(NoteId id);
    PatternNote* findNote(NoteId id);
    const PatternNote* findNote(NoteId id) const;
    bool updateNote(NoteId id, const PatternNote& updated);
    void clearNotes();

    EventId addAutomationPoint(AutomationPoint point);
    bool removeAutomationPoint(EventId id);
    AutomationPoint* findAutomationPoint(EventId id);
    const AutomationPoint* findAutomationPoint(EventId id) const;
    void clearAutomation();

    EventId addPitchBend(PitchBendEvent event);
    bool removePitchBend(EventId id);
    void clearPitchBends();

    EventId addAftertouch(AftertouchEvent event);
    bool removeAftertouch(EventId id);
    void clearAftertouch();

    EventId addProgramChange(ProgramChangeEvent event);
    bool removeProgramChange(EventId id);
    void clearProgramChanges();

    EventId addStepTrigger(StepTrigger trigger);
    bool removeStepTrigger(EventId id);
    void clearStepTriggers();

    const std::vector<PatternNote>& getNotes() const;
    const std::vector<AutomationPoint>& getAutomationPoints() const;
    const std::vector<PitchBendEvent>& getPitchBends() const;
    const std::vector<AftertouchEvent>& getAftertouchEvents() const;
    const std::vector<ProgramChangeEvent>& getProgramChanges() const;
    const std::vector<StepTrigger>& getStepTriggers() const;

    int getNoteCount() const;
    int getAutomationPointCount() const;
    int getPitchBendCount() const;
    int getAftertouchCount() const;
    int getProgramChangeCount() const;
    int getStepTriggerCount() const;

    void sortAll();
    PatternRange getRange() const;

private:
    static void clampNote(PatternNote& note);

    template <typename T>
    static T* findById(std::vector<T>& items, uint64_t id);

    template <typename T>
    static const T* findById(const std::vector<T>& items, uint64_t id);

    template <typename T>
    static bool eraseById(std::vector<T>& items, uint64_t id);

    NoteId nextNoteId();
    EventId nextEventId();

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

template <typename T>
T* Pattern::findById(std::vector<T>& items, uint64_t id) {
    auto it = std::find_if(items.begin(), items.end(), [id](const T& item) {
        return item.id == id;
    });
    return it != items.end() ? &(*it) : nullptr;
}

template <typename T>
const T* Pattern::findById(const std::vector<T>& items, uint64_t id) {
    auto it = std::find_if(items.begin(), items.end(), [id](const T& item) {
        return item.id == id;
    });
    return it != items.end() ? &(*it) : nullptr;
}

template <typename T>
bool Pattern::eraseById(std::vector<T>& items, uint64_t id) {
    auto it = std::find_if(items.begin(), items.end(), [id](const T& item) {
        return item.id == id;
    });
    if (it == items.end()) {
        return false;
    }
    items.erase(it);
    return true;
}
