#pragma once
#include <cstdint>

using PatternId = uint64_t;
using PatternClipId = uint64_t;
using ArrangementId = uint64_t;
using ArrangementLaneId = uint64_t;
using ChannelId = uint64_t;
using TrackId = uint64_t;
using BusId = uint64_t;
using NoteId = uint64_t;
using EventId = uint64_t;
using RackChannelId = uint64_t;
using ParameterId = uint64_t;
using MarkerId = uint64_t;

static constexpr PatternId kInvalidPatternId = 0;
static constexpr PatternClipId kInvalidPatternClipId = 0;
static constexpr ArrangementId kInvalidArrangementId = 0;
static constexpr ArrangementLaneId kInvalidArrangementLaneId = 0;
static constexpr ChannelId kInvalidChannelId = 0;
static constexpr TrackId kInvalidTrackId = 0;
static constexpr BusId kMasterBusId = 1;
static constexpr NoteId kInvalidNoteId = 0;
static constexpr EventId kInvalidEventId = 0;
static constexpr RackChannelId kInvalidRackChannelId = 0;
static constexpr MarkerId kInvalidMarkerId = 0;
static constexpr int kDefaultPPQN = 96;
static constexpr int kDefaultArrangementPPQN = 96;

