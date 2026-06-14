#pragma once

class GameProgressData {
    public:
        int getHomeLevel(void) const;
        void upHomeLevel(void);
        void talkCapNearHomeInWaterfall(void);
        void crashHome();
        bool isUnlockWorld(int) const;
        s32 mWaterfallWorldProgress;
};
