#pragma once

#include "al/util.hpp"
#include "al/scene/Scene.h"

#include "agl/DrawContext.h"
#include "agl/utl.h"

#include "game/System/GameSystem.h"
#include "game/HakoniwaSequence/HakoniwaSequence.h"

#include "sead/devenv/seadDebugFontMgrNvn.h"
#include "sead/gfx/seadPrimitiveRenderer.h"
#include "sead/gfx/seadTextWriter.h"
#include "sead/gfx/seadViewport.h"

#include "debug/DebugMenu.h"

#include "server/Client.hpp"

namespace debug {
    class DebugMenu;
}

class ImmOverlayMenu {
    public:
        friend class debug::DebugMenu;

        static void init(sead::Heap*, agl::DrawContext*);
        static void draw(sead::Viewport*, HakoniwaSequence*, bool isInGame);
    private:
        void drawOverlayMenu(sead::Viewport*, HakoniwaSequence*, bool isInGame);

        void drawChat(sead::Viewport*, HakoniwaSequence*, bool isInGame, int dispHeight);
        void drawBackground(int rows);

        void printf_colored(const char *message);

        ImmOverlayMenu(sead::Heap*, agl::DrawContext*);
        agl::DrawContext *context;
        sead::TextWriter *gTextWriter;

        static ImmOverlayMenu *sInstance;

        // TODO: are there better fonts available? Maybe a less buggy shader?
        inline static const char *DBG_FONT_PATH = "DebugData/Font/nvn_font_jis1.ntx";
        inline static const char *DBG_SHADER_PATH = "DebugData/Font/nvn_font_shader_jis1.bin";
        inline static const char *DBG_TBL_PATH = "DebugData/Font/nvn_font_jis1_tbl.bin";
};
