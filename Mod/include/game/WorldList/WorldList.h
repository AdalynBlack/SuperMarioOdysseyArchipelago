#pragma once

#include "sead/container/seadPtrArray.h"
#include "sead/prim/seadSafeString.hpp"

class WorldList {
    class StageDBEntry {
        public:
            sead::FixedSafeString<0x80> name;
            sead::FixedSafeString<0x40> category;
            int useScenarioNo;
    };

    class WorldListEntry {
        public:
            const char* mainStageName;
            const char* worldDevelopName;
            int worldScenarioNum;
            int clearMainScenarioNo;
            int afterEndingScenarioNo;
            int moonRockScenarioNo;
            int* mainQuestIndexes;
            sead::PtrArray<StageDBEntry> stageList;
    };

    public:
        const char *getWorldDevelopName(int) const;
        int tryFindWorldIndexByStageName(char const *stageName) const;
        int getMoonRockScenarioNo(int worldId) const;
        int findUseScenarioNo(const char *stageName) const;
        bool checkIsMainStage(char const *stageName) const;

        sead::PtrArray<WorldListEntry> mWorldList;
};
