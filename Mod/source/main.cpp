#include "main.hpp"
#include <cmath>
#include <math.h>
#include "al/execute/ExecuteOrder.h"
#include "al/execute/ExecuteTable.h"
#include "al/execute/ExecuteTableHolderDraw.h"
#include "al/nerve/Nerve.h"
#include "al/util/GraphicsUtil.h"
#include "container/seadSafeArray.h"
#include "game/GameData/GameDataHolderAccessor.h"
#include "game/Player/PlayerActorBase.h"
#include "game/Player/PlayerActorHakoniwa.h"
#include "game/Player/PlayerHackKeeper.h"
#include "game/Player/PlayerCostumeFunction.h"
#include "game/Player/PlayerCostumeInfo.h"
#include "game/Layouts/ShopLayoutInfo.h"
#include "game/Layouts/CommonHorizontalList.h"
#include "game/StageScene/StageSceneStatePauseMenu.h"
#include "al/layout/MenuSelectParts.h"
#include "heap/seadHeap.h"
#include "math/seadVector.h"
#include "server/Client.hpp"
#include "puppets/PuppetInfo.h"
#include "actors/PuppetActor.h"
#include "al/LiveActor/LiveActor.h"
#include "al/util.hpp"
#include "al/util/AudioUtil.h"
#include "al/util/CameraUtil.h"
#include "al/util/ControllerUtil.h"
#include "al/util/LiveActorUtil.h"
#include "al/util/HeapUtil.h"
#include "al/util/NerveUtil.h"
#include "al/layout/IUseLayout.h"
#include "ImmOverlayMenu.h"
#include "game/GameData/GameDataFunction.h"
#include "game/HakoniwaSequence/HakoniwaSequence.h"
#include "game/Player/PlayerFunction.h"
#include "game/StageScene/StageScene.h"
#include "helpers.hpp"
#include "layouts/HideAndSeekIcon.h"
#include "logger.hpp"
#include "rs/util.hpp"
#include "server/gamemode/GameModeBase.hpp"
#include "server/hns/HideAndSeekMode.hpp"
#include "server/gamemode/GameModeManager.hpp"
#include "sead/prim/seadSafeString.h"
#include "sead/prim/seadSafeString.hpp"
#include "game/Demo/DemoStateHackFirst.h"
#include "game/Actors/GrowFlowerPot.h"

static int pInfSendTimer = 0;
static int gameInfSendTimer = 0;
static int checksSyncTimer = 0;
static int updateCounterTimer = 0;
static int messageShiftTimer = 0;

void updatePlayerInfo(StageScene* stageScene, PlayerActorBase* playerBase, bool isYukimaru) {
    GameDataHolderAccessor holder = stageScene->mHolder;
    if (pInfSendTimer >= 5) {

        Client::sendPlayerInfPacket(playerBase, isYukimaru);

        if (!isYukimaru) {
            Client::sendHackCapInfPacket(((PlayerActorHakoniwa*)playerBase)->mHackCap);

            Client::sendCaptureInfPacket((PlayerActorHakoniwa*)playerBase);
        }
        
        if (Client::getCapturesFlag() && !isYukimaru) {
            PlayerHackKeeper* hackKeeper = playerBase->getPlayerHackKeeper();
            if(hackKeeper) {
                const char* hackName = hackKeeper->getCurrentHackName();
                if (hackName != nullptr && !Client::hasCapture(hackName)) {
                    if (!(al::isEqualString(hackName, "ElectricWire") && Client::getScenario(0) < 2
                        && GameDataFunction::getCurrentWorldId(holder) == 0)) {
                        //Client::setMessage(1, hackNamehackName);
                        if (!playerBase->getPlayerHackKeeper()->isActiveHackStartDemo()) {
                            bool tryEscape = false;
                            int nonKillCaptures[39] = {0, 1, 2, 3, 4, 5, 7, 9, 10, 11, 13, 14, 15, 16, 17, 18, 21, 22, 23, 24, 25, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39, 43, 44, 46, 48, 49, 51};
                            for (int i = 0; i < 39; i++) {
                                tryEscape =
                                    al::isEqualString(captureListNames[nonKillCaptures[i]], hackName);
                                if (tryEscape) {
                                    break;
                                }
                            }
                            if (tryEscape) {
                                playerBase->getPlayerHackKeeper()->cancelHack();
                            } else {
                                playerBase->getPlayerHackKeeper()->forceKillHack();
                            }
                        }
                    }
                }
            }
        }

        holder.mData->mGameDataFile->mBossSaveData->mIsAlreadyDeadGKLv1[0] = Client::getScenario(0) > 1;

        if (!(al::isEqualString(GameDataFunction::tryGetCurrentMainStageName(holder),
                              "CapWorldHomeStage") &&
              Client::getScenario(0) < 2) &&
            rs::isExistShineChipWatcher(playerBase) &&
            rs::getShineChipCount(playerBase) > 0) {
            Client::startShineChipCount();
        }
        pInfSendTimer = 0;
    }

    if (gameInfSendTimer >= 60) {
        // Check and prevent crashed home softlock
        if (GameDataFunction::isBossAttackedHome(holder)) {
            //Client::setMessage(1, GameDataFunction::getCurrentStageName(holder));
            if (strcmp(GameDataFunction::getCurrentStageName(holder), "BossRaidWorldHomeStage") ==
                0) {

                GameDataFunction::repairHomeByCrashedBoss(holder);
                GameDataFunction::crashHome(holder);
                // isGotShine crashes game here for some reason
                /*int ruinedCount = 0;
                if (GameDataFunction::isGotShine(holder, GameDataFunction::getWorldIndexBoss(),
                                                 0)) {
                    ruinedCount += 3;
                }

                for (int i = 1; i < 9; i++) {
                    if (GameDataFunction::isGotShine(holder, GameDataFunction::getWorldIndexBoss(),
                                                     i)) {
                        ruinedCount++;
                    }
                }
                if (ruinedCount < Client::getRaidCount()) {
                    GameDataFunction::repairHome(holder);
                } else {
                    GameDataFunction::bossAttackHome(holder);
                }*/
            } else {
                GameDataFunction::repairHome(holder); 
            }
        }

        // Edge case where game repairs odyssey in ruined but doesn't unlock bowser kingdom
        if (GameDataFunction::isRepairHomeByCrashedBoss(holder)) {
            GameDataFunction::unlockWorld(holder, GameDataFunction::getWorldIndexSky());
        }

        // Check for lost kingdom softlock state
        if (GameDataFunction::isCrashHome(holder)) {
            if (strcmp(GameDataFunction::getCurrentStageName(holder), "ClashWorldHomeStage") == 0) {
                int lostCount = 0;
                for (int i = 1; i < 25; i++) {
                    if (GameDataFunction::isGotShine(holder, GameDataFunction::getWorldIndexClash(), i))
                        lostCount++;
                }
                if (lostCount < Client::getWorldUnlockCount(GameDataFunction::getWorldIndexClash())) {
                    GameDataFunction::repairHome(holder);
                    GameDataFunction::unlockWorld(holder, GameDataFunction::getWorldIndexClash());
                } else {
                    GameDataFunction::crashHome(holder);
                }
            } else {
                GameDataFunction::repairHome(holder);
            }
        }

        // Death Link handling
        if (isInGame && !PlayerFunction::isPlayerDeadStatus(playerBase) && Client::isApDeath())
        {
            GameDataFunction::killPlayer(holder);
            playerBase->startDemoPuppetable();
            al::setVelocityZero(playerBase);
            rs::faceToCamera(playerBase);

            if (!isYukimaru) {
                ((PlayerActorHakoniwa*)playerBase)->mPlayerAnimator->endSubAnim();
                ((PlayerActorHakoniwa*)playerBase)->mPlayerAnimator->startAnimDead();
            } else {
                // The player can't be killed in the race, but we can trigger an instant loss instead
                GameDataFunction::loseRace(holder);
                if (stageScene &&
                        stageScene->mStageSceneStateYukimaruRace &&
                        stageScene->mStageSceneStateYukimaruRace->mRaceWatcher) {
                    ChangeStageInfo* loseStageInfo = stageScene->mStageSceneStateYukimaruRace->mRaceWatcher->mLoseStageInfo;

                    if (loseStageInfo)
                        GameDataFunction::tryChangeNextStage(holder, loseStageInfo);
                    else
                        GameDataFunction::returnPrevStage(holder);
                    stageScene->kill();
                }
            }

            Client::setApDeath(false);
        }

        if (isInGame && PlayerFunction::isPlayerDeadStatus(playerBase) && !Client::isDying())
        {
            Client::sendDeathlinkPacket();
            Client::setDying(true);
        }

        if (isInGame && !PlayerFunction::isPlayerDeadStatus(playerBase) && Client::isDying())
        {
            Client::setDying(false);
        }

        if (isInGame && updateCounterTimer >= 1800)
        {
            Client::startShineCount();
            updateCounterTimer = 0;
        }

        if (isYukimaru) {
            Client::sendGameInfPacket(holder);
        } else {
            Client::sendGameInfPacket((PlayerActorHakoniwa*)playerBase, holder);
        }

        gameInfSendTimer = 0;
    }

    pInfSendTimer++;
    gameInfSendTimer++;
    updateCounterTimer++;
}

// ------------- Hooks -------------


void drawMainHook(HakoniwaSequence *sequence, sead::Viewport *viewport, sead::DrawContext *drawContext) {
    Time::calcTime();  // this needs to be ran every frame, so running it here works

    ImmOverlayMenu::draw(viewport, sequence, isInGame);

    // Replicate the functionality of the original code
    // 2D Background (Main Surface)
    al::executeDraw(sequence->mLytKit, "２Ｄバック（メイン画面）");
}

bool isGrabShine(GameDataHolderAccessor accessor, int shineIdx) {
    GameDataFile::HintInfo* curHintInfo =
        &accessor.mData->mGameDataFile->mShineHintList[shineIdx];
    if (!curHintInfo->mIsGrand) {
        if (curHintInfo->mUniqueID == 205 && Client::getScenario(1) > 1 ||
            curHintInfo->mUniqueID == 129)
        {
            return true;
        }
        return Client::hasShine(curHintInfo->mUniqueID);
    }
    return false;
}

bool isGotShineRedirect(const Shine* curShine)
{
    GameDataHolderAccessor accessor = GameDataHolderAccessor(curShine);
    return isGrabShine(accessor, curShine->mShineIdx);
}

bool isGotShineByHintIdx(GameDataHolderAccessor accessor, int worldId, int hintIdx)
{
    if (accessor.mData->mGameDataFile) {
        GameDataFile::HintInfo* curHintInfo =
            accessor.mData->mGameDataFile->findShineByHintIdx(worldId, hintIdx);
        sead::FixedSafeString<60> str;
        str = "";
        str.append("World Id: ");
        str.append(intToCstr(worldId));
        str.append(" shineIdx: ");
        str.append(intToCstr(hintIdx));
        str.append(" Uid: ");
        str.append(intToCstr(curHintInfo->mUniqueID));
        str.append(" UnkBool: ");
        str.append(intToCstr(curHintInfo->unkBool1));
        str.append(" ???: ");
        str.append(intToCstr(curHintInfo->unkInt));
        str.append(" ???: ");
        str.append(intToCstr(curHintInfo->unkInt2));
        //Client::setMessage(1, str.cstr());
        if (curHintInfo) {
            return Client::hasShine(curHintInfo->mUniqueID);
        }
    }
    return false;
}

void sendShinePacket(GameDataHolderAccessor thisPtr, Shine* curShine) {

    GameDataFile::HintInfo* curHintInfo =
    &thisPtr.mData->mGameDataFile->mShineHintList[curShine->mShineIdx];

    Client::setRecentShine(curShine);

    if (curHintInfo->mUniqueID == 0) {
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "CapWorldHomeStage") == 0) {
            Client::sendCheckPacket(1086, -1);
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "SandWorldHomeStage") == 0) {
            Client::sendCheckPacket(1096, -1);
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "LakeWorldHomeStage") == 0) {
            Client::sendCheckPacket(1094, -1);
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "ForestWorldHomeStage") == 0) {
            Client::sendCheckPacket(1089, -1);
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "CityWorldHomeStage") == 0) {
            Client::sendCheckPacket(1088, -1);
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "SnowWorldHomeStage") == 0) {
            Client::sendCheckPacket(1087, -1);
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "SeaWorldHomeStage") == 0) {
            Client::sendCheckPacket(1095, -1);
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "LavaWorldHomeStage") == 0) {
            Client::sendCheckPacket(1090, -1);
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "SkyWorldHomeStage") == 0) {
            Client::sendCheckPacket(1091, -1);
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "MoonWorldHomeStage") == 0) {
            Client::sendCheckPacket(1165, -1);
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "PeachWorldHomeStage") == 0) {
            Client::sendCheckPacket(1152, -1);
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "Special1WorldHomeStage") == 0) {
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(thisPtr),
                       "WaterfallWorldHomeStage") == 0) {
                Client::sendCheckPacket(1132, -1);
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(thisPtr),
                       "LakeWorldHomeStage") == 0) {
                Client::sendCheckPacket(1128, -1);
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(thisPtr),
                       "CloudWorldHomeStage") == 0) {
                Client::sendCheckPacket(1124, -1);
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(thisPtr),
                       "ClashWorldHomeStage") == 0) {
                Client::sendCheckPacket(1126, -1);
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(thisPtr),
                       "CityWorldHomeStage") == 0) {
                Client::sendCheckPacket(1130, -1);
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(thisPtr),
                       "SnowWorldHomeStage") == 0) {
                Client::sendCheckPacket(1129, -1);
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(thisPtr),
                       "SeaWorldHomeStage") == 0) {
                Client::sendCheckPacket(1127, -1);
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(thisPtr),
                       "LavaWorldHomeStage") == 0) {
                Client::sendCheckPacket(1123, -1);
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(thisPtr),
                       "BossRaidWorldHomeStage") == 0) {
                Client::sendCheckPacket(1125, -1);
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(thisPtr),
                       "PeachWorldHomeStage") == 0) {
                Client::sendCheckPacket(1131, -1);
            }
        }
    } else {
        Client::sendCheckPacket(curHintInfo->mUniqueID, -1);
    }
    // Add some way to sync shinechecks grabbed before connecting, probably handle on connect or something
    Client::addShine(curHintInfo->mUniqueID);
}

void sendItemPacket(GameDataFile thisPtr, ShopItem::ItemInfo* info, bool flag) {

    int itemType = static_cast<int>(info->mType);

    switch (itemType) 
    { 
        case 0:
            Client::sendCheckPacket(getIndexCostumeList(info->mName), itemType);
            break;
        case 1:
            Client::sendCheckPacket(getIndexCostumeList(info->mName), itemType);
            break;
        case 2:
            Client::sendCheckPacket(getIndexSouvenirList(info->mName), itemType);
            break;
        case 3:
            Client::sendCheckPacket(getIndexStickerList(info->mName), itemType);
            break;
    }
    
    Client::addItem(info);
 }

void sendCollectPacket(GameDataHolderAccessor thisPtr, al::PlacementId* placementId)
{
    if (Client::getRegionalsFlag())
    {
        sead::FixedSafeString<0x20> placementString;
        placementId->makeString(&placementString);
        Client::sendCheckPacket(4, placementString.cstr(), GameDataFunction::getCurrentStageName(thisPtr));
    }
    else
    {
        GameDataFunction::addCoinCollect(thisPtr, placementId);
    }
    
    // Add flag in client to determine when option is disabled and pass regularly to GameDataFunction
}

int getUnlockShineNum(GameDataHolder* thisPtr, bool* unkBool, int worldId)
{
    if (worldId < 1 || worldId > 16)
    {
        worldId = 0;
    }
    //*unkBool = thisPtr->mGameDataFile->mGameProgressData->isUnlockWorld(worldId);
    //if (*unkBool) {
    return Client::getWorldUnlockCount(worldId);
    //}
    //return -1;
}

int getUnlockShineNumByAccessor(bool* unkBool, GameDataHolderAccessor accessor)
{
    int worldId = accessor.mData->mGameDataFile->mCurWorldID;
    if (worldId < 1 || worldId > 16)
    {
        worldId = 0;
    }
    //*unkBool = GameDataFunction::isUnlockedWorld(accessor, worldId);
    //if (*unkBool) {
        return Client::getWorldUnlockCount(worldId);
    //}
    return -1;
}

int getUnlockShineNumByGameDataFile(GameDataFile* file, bool* unkBool)
{
    int worldId = file->mCurWorldID;
    if (worldId < 1 || worldId > 16) {
        worldId = 0;
    }
    //*unkBool = file->mGameProgressData->isUnlockWorld(worldId);
    //if (*unkBool) {
    return Client::getWorldUnlockCount(worldId);
    //}
    //return -1;
}

int getUnlockShineNumByWorldId(bool* unkBool, GameDataHolder* thisPtr, int worldId)
{
    return getUnlockShineNum(thisPtr, unkBool, worldId);
}

void onGrandShineStageChange(GameDataHolderWriter holder, ChangeStageInfo const* stageInfo) 
{
    Client::sendStage(holder, stageInfo);
}

void onStageChange(GameDataFile *file, const ChangeStageInfo* stageInfo, int param2)
{
    //Client::setMessage(1, stageInfo->changeStageId.cstr());
    // Add Wooded shop moon warp
    if (!(al::isEqualString(stageInfo->changeStageId.cstr(), "obj846") ||
        al::isEqualString(stageInfo->changeStageId.cstr(), "obj1084"))) {
        if (isPartOf(stageInfo->changeStageName.cstr(), "WorldHomeStage")) {
            if (Client::setScenario(stageInfo->changeStageName.cstr(), stageInfo->scenarioNo)) {
                //Client::setMessage(2, "attempting send to correct scenario");
                Client::sendCorrectScenario(stageInfo);

            } else {
                //Client::setMessage(2, "setScenario false");
                file->changeNextStage(stageInfo, param2);
            }
        } else {
            // Non world transitions
            //Client::setMessage(2, "non world transition");
            file->changeNextStage(stageInfo, param2);
        }
    } else {
        // Catch cap and cascade shop moons
        //Client::setMessage(2, "Shop moon stageID caught");
        file->changeNextStage(stageInfo, param2);
    }
    
}

void setShineLabel(al::IUseLayout* layout, const char* elementLabel)
{
    al::setPaneStringFormat(layout, elementLabel, Client::getShineReplacementText());
}

void setShineColor(Shine* thisPtr, char* stageName, int color, bool isSetMtpColor)
{
    // Get color here using shine unique id
    //Client::setMessage(1, "Set custom shine color");
    int storedColor = Client::getShineColor(thisPtr);
    rs::setStageShineAnimFrame((al::LiveActor*)thisPtr, stageName, storedColor, isSetMtpColor);
}

void setShineModelColor(Shine* thisPtr, char* stageName, int color, bool isSetMtpColor) 
{
    // Get color here using shine unique id
    //Client::setMessage(1, "Set custom other shine color");
    int storedColor = Client::getShineColor(thisPtr);
    rs::setStageShineAnimFrame(thisPtr->mModelShine, stageName, storedColor, isSetMtpColor);
}

const char16_t* getShopItemMessage(al::IUseMessageSystem const* messageSystem, char const* fileName, char const* key) 
{
    const char16_t* msg =  Client::getShopReplacementText(fileName, key);
    sead::WFixedSafeString<200> confirm;
    confirm = confirm.cEmptyString;
    confirm.append(msg);
    if (!confirm.isEmpty())
    {
        return msg;
    }
    // Default to base game text if no ap text exists
    return al::getSystemMessageString(messageSystem, fileName, key);
}

// Returns false if the item should be purchasable, true if the item should be greyed out
bool isBuyItems(ShopItem::ItemInfo* itemInfo) {
    // Add a collected outfits, gifts, stickers based implementation similar to shinechecks
    
    return false;
}

//isExistInHackDictionary for capture tracking
void onAddHack(GameDataHolderWriter writer,const char* hackName)
{
    if (Client::getCapturesFlag()) {
        //Client::setMessage(2, hackName);
        Client::sendCheckPacket(getIndexCaptureList(hackName), 5);
    } else {
        GameDataFunction::addHackDictionary(writer, hackName);
    }
}

void canEndHack(al::LiveActor* actor)
{
    if (actor != nullptr)
    {
        PlayerHackKeeper::endHackStartDemo(actor);
    }
}

void onStartHack(PlayerHackKeeper* keeper, al::HitSensor* hitSensor1, al::HitSensor* hitSensor2, al::LiveActor* actor) 
{
    keeper->startHack(hitSensor1, hitSensor2, actor);
}

bool growOnPlant(GrowFlowerPot* thisPtr) {
    rs::setGrowFlowerTime(thisPtr, thisPtr->mPlacementId, 3600000);
    return al::isActionEnd(thisPtr);
}

// _ZN16HakoniwaSequence15exeBootLoadDataEv = 0x50F29C - 0x50F304
void onNewGameDemoStart(char* name, bool unkBool) {
    for (int i = 0; i < 18; i++) {
        Client::setScenario(i, 1);
    }

    for (int i = 0; i < 25; i++) {
        Client::setShineChecks(i, 0);
    }

    for (int i = 0; i < 12; i++) {
        Client::setOutfitChecks(i, 0);
    }

    for (int i = 0; i < 4; i++) {
        Client::setStickerChecks(i, 0);
    }

    for (int i = 0; i < 5; i++) {
        Client::setSouvenirChecks(i, 0);
    }

    for (int i = 0; i < 8; i++) {
        Client::setCaptureChecks(i, 0);
    }

    Client::setCheckIndex(-1);

    //al::initActorWithArchiveName(thisPtr, info, str, name);
    al::createSceneHeap(name, unkBool);
    return;
}

// First time entering lost in demo from cloud
void onUnlockLost(GameDataHolderWriter writer, int worldIndex)
{
    // Send Beat Bowser in Cloud location
    Client::sendCheckPacket(2500, -1);
    //Client::setScenario(GameDataFunction::getWorldIndexCloud(), 2);
    
    GameDataFunction::unlockWorld(writer, worldIndex);

    return;
}

void onCreditsStart(al::Scene* thisPtr, const al::SceneInitInfo info) {

    Client::sendCheckPacket(2499, -1);
    
    thisPtr->initDrawSystemInfo(info);
    return;
}

bool skipHackCutscene(DemoStateHackFirst* thisPtr, IUsePlayerHack** param_1, al::SensorMsg* param_2,
    al::HitSensor* param_3, al::HitSensor* param_4)
{
    return false;
}

void stageInitHook(al::ActorInitInfo *info, StageScene *curScene, al::PlacementInfo const *placement, al::LayoutInitInfo const *lytInfo, al::ActorFactory const *factory, al::SceneMsgCtrl *sceneMsgCtrl, al::GameDataHolderBase *dataHolder) {
    al::initActorInitInfo(info, curScene, placement, lytInfo, factory, sceneMsgCtrl,
                          dataHolder);

    Client::clearArrays();

    Client::setSceneInfo(*info, curScene);

    if (GameModeManager::instance()->getGameMode() != NONE) {
        GameModeInitInfo initModeInfo(info, curScene);
        initModeInfo.initServerInfo(GameModeManager::instance()->getGameMode(), Client::getPuppetHolder());

        GameModeManager::instance()->initScene(initModeInfo);
    }

    Client::sendGameInfPacket(info->mActorSceneInfo.mSceneObjHolder);
    GameDataHolderAccessor accessor = GameDataHolderAccessor(info->mActorSceneInfo.mSceneObjHolder);
    if (!GameDataFunction::isHomeShipStage(accessor.mData)) {
        Client::sendChangeStagePacket(info->mActorSceneInfo.mSceneObjHolder);
    }
    int worldId = GameDataFunction::getCurrentWorldId(info->mActorSceneInfo.mSceneObjHolder);
    int worldScenario =
        GameDataFunction::getWorldScenarioNo(info->mActorSceneInfo.mSceneObjHolder, worldId);
    if (worldScenario > Client::getScenario(worldId))
    {
        Client::setScenario(worldId, worldScenario);
    }

}

PlayerCostumeInfo *setPlayerModel(al::LiveActor *player, const al::ActorInitInfo &initInfo, const char *bodyModel, const char *capModel, al::AudioKeeper *keeper, bool isCloset) {
    Client::sendCostumeInfPacket(bodyModel, capModel);
    return PlayerFunction::initMarioModelActor(player, initInfo, bodyModel, capModel, keeper, isCloset);
}

al::SequenceInitInfo* initInfo;

ulong constructHook() {  // hook for constructing anything we need to globally be accesible

    __asm("STR X21, [X19,#0x208]"); // stores WorldResourceLoader into HakoniwaSequence

    __asm("MOV %[result], X20"
          : [result] "=r"(
              initInfo));  // Save our scene's init info to a gloabl ptr so we can access it later

    Client::createInstance(al::getCurrentHeap());
    GameModeManager::createInstance(al::getCurrentHeap()); // Create the GameModeManager on the current al heap

    return 0x20;
}

bool threadInit(HakoniwaSequence *mainSeq) {  // hook for initializing client class

    al::LayoutInitInfo lytInfo = al::LayoutInitInfo();

    al::initLayoutInitInfo(&lytInfo, mainSeq->mLytKit, 0, mainSeq->mAudioDirector, initInfo->mSystemInfo->mLayoutSys, initInfo->mSystemInfo->mMessageSys, initInfo->mSystemInfo->mGamePadSys);

    Client::instance()->init(lytInfo, mainSeq->mGameDataHolder);

    return GameDataFunction::isPlayDemoOpening(mainSeq->mGameDataHolder);
}


bool hakoniwaSequenceHook(HakoniwaSequence* sequence) {
    bool isFirstStep = al::isFirstStep(sequence);
    StageScene* stageScene = (StageScene*)sequence->curScene;

    if (!stageScene) {
        isInGame = false;
        return isFirstStep;
    }

    al::PlayerHolder *pHolder = al::getScenePlayerHolder(stageScene);
    PlayerActorBase* playerBase = al::tryGetPlayerActor(pHolder, 0);
    
    bool isYukimaru = !playerBase->getPlayerInfo();

    isInGame = !stageScene->isPause();

    GameModeManager::instance()->setPaused(stageScene->isPause());
    Client::setStageInfo(stageScene->mHolder);

    Client::update();

    updatePlayerInfo(stageScene, playerBase, isYukimaru);

    debug::DebugMenu::updateInputs(stageScene);

    return isFirstStep;
}

void seadPrintHook(const char *fmt, ...)
{
    va_list args;
	va_start(args, fmt);

    Logger::log(fmt, args);
    va_end(args);
}

void setAPNerve(StageSceneStatePauseMenu *menu) {
    MenuSelectParts *selectParts = menu->mSelectParts;
    int TitleIndices[] = {0, 1, 2, 3, 5, 6};
    int PauseIndices[] = {0, 1, 3, 4, 5, 6};

    int *indices = PauseIndices;

    if (selectParts->isMenuMain != false) {
        indices = TitleIndices;
    }

    if (indices[selectParts->mCursorItemIdx] == 6 && al::isNerve(selectParts, &nrvMenuSelectPartsHide)) {
        al::setNerve(menu, &nrvStageSceneStatePauseMenuServerConfig);
    } else {
        al::updateKitListPost(menu->mHost);
    }
}
