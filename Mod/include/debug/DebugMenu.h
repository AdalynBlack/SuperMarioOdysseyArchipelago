#pragma once

#include "al/scene/Scene.h"
#include "al/util/ControllerUtil.h"
#include "al/util/MathUtil.h"

#include "game/HakoniwaSequence/HakoniwaSequence.h"
#include "game/StageScene/StageScene.h"
#include "game/System/Application.h"

#include "sead/devenv/seadDebugFontMgrNvn.h"
#include "sead/gfx/seadViewport.h"
#include "sead/gfx/seadTextWriter.h"

#include "ImmOverlayMenu.h"
#include "debug/DebugState.h"
#include "server/Client.hpp"

namespace debug {
	class DebugMenu {
		public:
			static void updateInputs(StageScene*);
			static bool draw(sead::Viewport *viewport, HakoniwaSequence *sequence, bool isInGame, int dispHeight);
		private:
			static void handleInputZR(StageScene *stageScene);
			static void handleInputZL(StageScene *stageScene);
			static void handleInputL(StageScene *stageScene);
			static void drawBackground();

			static int pageIndex;
			static int puppetIndex;
			static bool isDisableMusic;
			static const int maxPages = 2;
	};
}
