#pragma once

#include "audio/Ids.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <utility>

// Engine audio architecture:
//
// Arrangement contains PatternClips
// PatternClips say where in an arrangement a pattern goes
// Pattern contains Notes and Midi Events
// PatternNotes have a target channel
// Channel owns a generator (plugin or sample) outputs to track
// Tracks take inputs through inserts and outputs to a bus
// Buses sum tracks, the master bus being the final (required) stop

enum class ClipColor : uint32_t {
    Default = 0xFF808080,
    Red     = 0xFFFF5F57,
    Orange  = 0xFFFFA94D,
    Yellow  = 0xFFFFE066,
    Green   = 0xFF69DB7C,
    Cyan    = 0xFF66D9E8,
    Blue    = 0xFF74C0FC,
    Purple  = 0xFFB197FC,
    Pink    = 0xFFFF87B6
};

struct TimeSignature {
    int numerator = 4;
    int denominator = 4;
};

struct TempoMarker {
    int tick = 0;
    double bpm = 120.0;
};

struct ArrangementMarker {
    MarkerId id = kInvalidMarkerId;
    std::string name;
    int tick = 0;
    bool selected = false;
};

struct PatternClip {
    PatternClipId id = kInvalidPatternClipId;
    PatternId patternId = kInvalidPatternId;

    ArrangementLaneId laneId = kInvalidArrangementLaneId;

    int startTick = 0;
    int lengthTicks = kDefaultArrangementPPQN * 16;
    int offsetTick = 0;

    bool muted = false;
    bool selected = false;

    ClipColor color = ClipColor::Default;
    std::string nameOverride;

    bool containsTick(int tick) const {
        return tick >= startTick && tick < (startTick + lengthTicks);
    }

    int endTick() const {
        return startTick + lengthTicks;
    }
};

struct ArrangementLane {
    ArrangementLaneId id = kInvalidArrangementLaneId;
    std::string name;
    bool muted = false;
    bool solo = false;
    bool collapsed = false;
    bool selected = false;
};

struct LoopRange {
    bool enabled = false;
    int startTick = 0;
    int endTick = 0;

    bool isValid() const {
        return endTick > startTick;
    }

    bool contains(int tick) const {
        return enabled && tick >= startTick && tick < endTick;
    }
};

class Arrangement {
public:
    Arrangement() = default;

    Arrangement(ArrangementId id, std::string name, int lengthTicks = kDefaultArrangementPPQN * 64)
        : m_id(id), m_name(std::move(name)), m_lengthTicks(std::max(1, lengthTicks)) {}

    ArrangementId getId() const { return m_id; }

    const std::string& getName() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    int getLengthTicks() const { return m_lengthTicks; }
    void setLengthTicks(int ticks) { m_lengthTicks = std::max(1, ticks); }

    int getPpqn() const { return m_ppqn; }
    void setPpqn(int ppqn) { m_ppqn = std::max(1, ppqn); }

    double getBaseTempoBpm() const { return m_baseTempoBpm; }
    void setBaseTempoBpm(double bpm) { m_baseTempoBpm = std::max(1.0, bpm); }

    const TimeSignature& getTimeSignature() const { return m_timeSignature; }
    void setTimeSignature(TimeSignature sig) {
        sig.numerator = std::max(1, sig.numerator);
        sig.denominator = std::max(1, sig.denominator);
        m_timeSignature = sig;
    }

    const LoopRange& getLoopRange() const { return m_loopRange; }
    void setLoopRange(const LoopRange& range) { m_loopRange = range; }

    bool isEmpty() const {
        return m_clips.empty() && m_markers.empty() && m_tempoMarkers.empty();
    }

    ArrangementLaneId addLane(std::string name = {}) {
        ArrangementLane lane;
        lane.id = nextLaneId();
        lane.name = name.empty() ? defaultLaneName() : std::move(name);
        m_lanes.push_back(lane);
        return lane.id;
    }

    bool removeLane(ArrangementLaneId id) {
        auto it = std::find_if(m_lanes.begin(), m_lanes.end(), [id](const ArrangementLane& lane) {
            return lane.id == id;
        });
        if (it == m_lanes.end()) return false;

        m_lanes.erase(it);

        m_clips.erase(
            std::remove_if(m_clips.begin(), m_clips.end(), [id](const PatternClip& clip) {
                return clip.laneId == id;
            }),
            m_clips.end()
        );

        return true;
    }

    ArrangementLane* findLane(ArrangementLaneId id) {
        auto it = std::find_if(m_lanes.begin(), m_lanes.end(), [id](const ArrangementLane& lane) {
            return lane.id == id;
        });
        return it != m_lanes.end() ? &(*it) : nullptr;
    }

    const ArrangementLane* findLane(ArrangementLaneId id) const {
        auto it = std::find_if(m_lanes.begin(), m_lanes.end(), [id](const ArrangementLane& lane) {
            return lane.id == id;
        });
        return it != m_lanes.end() ? &(*it) : nullptr;
    }

    PatternClipId addClip(PatternClip clip) {
        clip.id = nextClipId();
        clip.startTick = std::max(0, clip.startTick);
        clip.lengthTicks = std::max(1, clip.lengthTicks);
        clip.offsetTick = std::max(0, clip.offsetTick);

        if (clip.laneId == kInvalidArrangementLaneId) {
            if (m_lanes.empty()) {
                clip.laneId = addLane();
            } else {
                clip.laneId = m_lanes.front().id;
            }
        }

        m_clips.push_back(clip);
        m_clipsDirty = true;
        return clip.id;
    }

    PatternClipId addClip(
        PatternId patternId,
        int startTick,
        int lengthTicks,
        ArrangementLaneId laneId = kInvalidArrangementLaneId,
        int offsetTick = 0
    ) {
        PatternClip clip;
        clip.patternId = patternId;
        clip.startTick = startTick;
        clip.lengthTicks = lengthTicks;
        clip.laneId = laneId;
        clip.offsetTick = offsetTick;
        return addClip(clip);
    }

    bool removeClip(PatternClipId id) {
        auto it = std::find_if(m_clips.begin(), m_clips.end(), [id](const PatternClip& clip) {
            return clip.id == id;
        });
        if (it == m_clips.end()) return false;
        m_clips.erase(it);
        return true;
    }

    PatternClip* findClip(PatternClipId id) {
        auto it = std::find_if(m_clips.begin(), m_clips.end(), [id](const PatternClip& clip) {
            return clip.id == id;
        });
        return it != m_clips.end() ? &(*it) : nullptr;
    }

    const PatternClip* findClip(PatternClipId id) const {
        auto it = std::find_if(m_clips.begin(), m_clips.end(), [id](const PatternClip& clip) {
            return clip.id == id;
        });
        return it != m_clips.end() ? &(*it) : nullptr;
    }

    std::vector<PatternClip*> getClipsAtTick(int tick) {
        std::vector<PatternClip*> result;
        for (auto& clip : m_clips) {
            if (clip.containsTick(tick)) {
                result.push_back(&clip);
            }
        }
        return result;
    }

    std::vector<const PatternClip*> getClipsAtTick(int tick) const {
        std::vector<const PatternClip*> result;
        for (const auto& clip : m_clips) {
            if (clip.containsTick(tick)) {
                result.push_back(&clip);
            }
        }
        return result;
    }

    std::vector<PatternClip*> getClipsInRange(int startTick, int endTick) {
        std::vector<PatternClip*> result;
        for (auto& clip : m_clips) {
            if (clip.startTick < endTick && clip.endTick() > startTick) {
                result.push_back(&clip);
            }
        }
        return result;
    }

    std::vector<const PatternClip*> getClipsInRange(int startTick, int endTick) const {
        std::vector<const PatternClip*> result;
        for (const auto& clip : m_clips) {
            if (clip.startTick < endTick && clip.endTick() > startTick) {
                result.push_back(&clip);
            }
        }
        return result;
    }

    void clearClips() {
        m_clips.clear();
        m_clipsDirty = false;
    }

    MarkerId addMarker(std::string name, int tick) {
        ArrangementMarker marker;
        marker.id = nextMarkerId();
        marker.name = std::move(name);
        marker.tick = std::max(0, tick);
        m_markers.push_back(marker);
        m_markersDirty = true;
        return marker.id;
    }

    bool removeMarker(MarkerId id) {
        auto it = std::find_if(m_markers.begin(), m_markers.end(), [id](const ArrangementMarker& marker) {
            return marker.id == id;
        });
        if (it == m_markers.end()) return false;
        m_markers.erase(it);
        return true;
    }

    TempoMarker& addTempoMarker(int tick, double bpm) {
        TempoMarker marker;
        marker.tick = std::max(0, tick);
        marker.bpm = std::max(1.0, bpm);
        m_tempoMarkers.push_back(marker);
        m_tempoMarkersDirty = true;
        return m_tempoMarkers.back();
    }

    void clearTempoMarkers() {
        m_tempoMarkers.clear();
        m_tempoMarkersDirty = false;
    }

    const std::vector<ArrangementLane>& getLanes() const { return m_lanes; }
    const std::vector<PatternClip>& getClips() const { return m_clips; }
    const std::vector<ArrangementMarker>& getMarkers() const { return m_markers; }
    const std::vector<TempoMarker>& getTempoMarkers() const { return m_tempoMarkers; }

    int getLaneCount() const { return static_cast<int>(m_lanes.size()); }
    int getClipCount() const { return static_cast<int>(m_clips.size()); }
    int getMarkerCount() const { return static_cast<int>(m_markers.size()); }

    void sortAll() {
        if (m_clipsDirty) {
            std::sort(m_clips.begin(), m_clips.end(), [](const PatternClip& a, const PatternClip& b) {
                if (a.laneId != b.laneId) return a.laneId < b.laneId;
                if (a.startTick != b.startTick) return a.startTick < b.startTick;
                if (a.patternId != b.patternId) return a.patternId < b.patternId;
                return a.id < b.id;
            });
            m_clipsDirty = false;
        }

        if (m_markersDirty) {
            std::sort(m_markers.begin(), m_markers.end(), [](const ArrangementMarker& a, const ArrangementMarker& b) {
                if (a.tick != b.tick) return a.tick < b.tick;
                return a.id < b.id;
            });
            m_markersDirty = false;
        }

        if (m_tempoMarkersDirty) {
            std::sort(m_tempoMarkers.begin(), m_tempoMarkers.end(), [](const TempoMarker& a, const TempoMarker& b) {
                return a.tick < b.tick;
            });
            m_tempoMarkersDirty = false;
        }
    }

    int getTicksPerBeat() const {
        return m_ppqn;
    }

    int getTicksPerBar() const {
        return m_ppqn * m_timeSignature.numerator;
    }

private:
    ArrangementLaneId nextLaneId() { return m_nextLaneId++; }
    PatternClipId nextClipId() { return m_nextClipId++; }
    MarkerId nextMarkerId() { return m_nextMarkerId++; }

    std::string defaultLaneName() const {
        return "Lane " + std::to_string(static_cast<int>(m_lanes.size()) + 1);
    }

    ArrangementId m_id = kInvalidArrangementId;
    std::string m_name;
    int m_lengthTicks = kDefaultArrangementPPQN * 64;
    int m_ppqn = kDefaultArrangementPPQN;

    double m_baseTempoBpm = 120.0;
    TimeSignature m_timeSignature;
    LoopRange m_loopRange;

    std::vector<ArrangementLane> m_lanes;
    std::vector<PatternClip> m_clips;
    std::vector<ArrangementMarker> m_markers;
    std::vector<TempoMarker> m_tempoMarkers;

    ArrangementLaneId m_nextLaneId = 1;
    PatternClipId m_nextClipId = 1;
    MarkerId m_nextMarkerId = 1;

    bool m_clipsDirty = false;
    bool m_markersDirty = false;
    bool m_tempoMarkersDirty = false;
};
