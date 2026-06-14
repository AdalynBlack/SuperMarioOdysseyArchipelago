#include "al/nerve/HostStateBase.h"

class RaceWatcher
{
    public:
        long* _8;
        long* _10;
        long* _18;
        long* _20;
        long* _28;
        long* _30;
        long* _38;
        long* _40;
        long* _48;
        long* _50;
        long* _58;
        long* _60;
        long* _68;
        long* _70;
        long* _78;
        long* _80;
        long* _88;
        long* _90;
        long* _98;
        long* _100;
        long* _108;
        long* _110;
        long* _118;
        long* _120;
        long* _128;
        long* _130;
        long* _138;
        long* _140;
        ChangeStageInfo* mLoseStageInfo; // 0x148
};

class StageSceneStateYukimaruRace : public al::HostStateBase<al::Scene>
{
    public:
        bool mAlreadyHasMoon; // 0x20
        unsigned long* mRaceIntroCamera; // 0x28
        unsigned long* mRaceCountDown; // 0x30
        unsigned long* mStageSceneStateRacePause; // 0x38
        unsigned long* mStageSceneStateSnapShow; // 0x40
        unsigned long* mRaceLayout; // 0x48
        unsigned long* mPlayer; // 0x50
        RaceWatcher* mRaceWatcher; // 0x58
        int mPlayerFinalRank; // 0x60
};

