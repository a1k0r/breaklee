#pragma once

#include "Base.h"
#include "Constants.h"

EXTERN_C_BEGIN

#pragma pack(push, 1)

struct _RTEventPassMissionSlot {
    Int32 Group;
    Int32 Index;
    Int32 Counter;
};

struct _RTEventPassRewardSlot {
    Int32 Group;
    Int32 Index;
    Int16 Status;
};

struct _RTEventPassInfo {
    Timestamp StartDate;
    Timestamp EndDate;
    Int32 UnknownA5;
    Int32 MissionSlotCount;
    Int32 RewardSlotCount;
};

struct _RTCharacterEventPassInfo {
    struct _RTEventPassInfo Info;
    struct _RTEventPassMissionSlot MissionSlots[RUNTIME_CHARACTER_MAX_EVENT_PASS_MISSION_SLOT_COUNT];
    struct _RTEventPassRewardSlot RewardSlots[RUNTIME_CHARACTER_MAX_EVENT_PASS_REWARD_SLOT_COUNT];
};

#pragma pack(pop)

EXTERN_C_END