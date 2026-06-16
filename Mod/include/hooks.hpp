#include "agl/DrawContext.h"

#include "al/byaml/ByamlIter.h"
#include "al/byaml/writer/ByamlWriter.h"
#include "al/execute/ExecuteDirector.h"

#include "game/Actors/WorldEndBorderKeeper.h"
#include "game/StageScene/StageSceneStateServerConfig.hpp"
#include "game/System/GameSystem.h"

#include "ImmOverlayMenu.h"
#include "server/hns/HideAndSeekMode.hpp"

void hookGameSystemInit(GameSystem *gameSystem);
bool registerShineToList(Shine* shineActor);
bool comboBtnHook(int port);
void saveWriteHook(al::ByamlWriter* saveByml);
bool saveReadHook(int* padRumbleInt, al::ByamlIter const& saveByml, char const* padRumbleKey);
void saveFileWriteHook(al::ByamlWriter* saveByaml);
bool saveFileReadHook(al::ByamlIter *saveByaml, bool* firstNetworkBool, char const* firstNetworkKey);
void startCounterHook(CoinCounter* thisPtr);
bool modeE3Hook();
void playGuideEndHook(al::SimpleLayoutAppearWaitEnd* thisPtr);
void overrideNerveHook(StageSceneStatePauseMenu* thisPtr, al::Nerve* nrvSet);

void initStateHook(StageSceneStatePauseMenu *thisPtr, char const *stateName, al::Scene *host, al::LayoutInitInfo const &initInfo, FooterParts *footer,
                   GameDataHolder *data, bool unkBool);
void initNerveStateHook(StageSceneStatePauseMenu* stateParent, StageSceneStateOption* stateOption,
                        al::Nerve const* executingNerve, char const* stateName);

void initHackCapHook(al::LiveActor *cappy);
al::PlayerHolder* createTicketHook(StageScene* curScene);

bool borderPullBackHook(WorldEndBorderKeeper* thisPtr);
void drawTableHook(al::ExecuteDirector* thisPtr, const al::ExecuteSystemInitInfo &initInfo);
