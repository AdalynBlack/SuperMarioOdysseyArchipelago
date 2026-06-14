#pragma once

#include <string>
#include <cstring>
#include "types.h"

#include "sead/math/seadVector.h"
#include "sead/math/seadQuat.hpp"

#include "al/util.hpp"

#include "logger.hpp"
#include "puppets/PuppetInfo.h"

#include "game/GameData/GameDataFunction.h"

bool isPartOf(const char* w1, const char* w2);

int indexOf(char *w1, char c1);

void logVector(const char* vectorName, sead::Vector3f vector);

void logQuat(const char *quatName, sead::Quatf quat);

sead::Vector3f QuatToEuler(sead::Quatf *quat);

float vecMagnitude(sead::Vector3f const &input);

float quatAngle(sead::Quatf const &q1, sead::Quatf &q2);

bool isInCostumeList(const char *costumeName);
int getIndexCostumeList(const char *costumeName);

int getIndexStickerList(const char *stickerName);
int getIndexSouvenirList(const char *souvenirName);
int getIndexCaptureList(const char *captureName);
int getIndexMoonItemList(const char *moonItemName);

const char *tryGetPuppetCapName(PuppetInfo *info);
const char* tryGetPuppetBodyName(PuppetInfo* info);

const char* tryConvertName(const char* className);

void killMainPlayer(al::LiveActor* actor);
void killMainPlayer(PlayerActorHakoniwa* mainPlayer);

const char* intToCstr(int number);

__attribute__((used)) static const char* costumeNames[] = {
    "Mario",
    "MarioTailCoat",
    "MarioPrimitiveMan",
    "MarioPoncho",
    "MarioGunman",
    "MarioSwimwear",
    "MarioExplorer",
    "MarioScientist",
    "MarioPilot",
    "MarioMaker",
    "MarioGolf",
    "MarioSnowSuit",
    "MarioAloha",
    "MarioSailor",
    "MarioCook",
    "MarioPainter",
    "MarioArmor",
    "MarioHappi",
    "MarioSpaceSuit",
    "Mario64",
    "MarioShopman",
    "MarioNew3DS",
    "MarioMechanic",
    "MarioSuit",
    "MarioPirate",
    "MarioClown",
    "MarioFootball",
    "MarioColorClassic",
    "MarioColorLuigi",
    "MarioColorWario",
    "MarioColorWaluigi",
    "MarioColorGold",
    "MarioDoctor",
    "MarioDiddyKong",
    "MarioKoopa",
    "MarioPeach",
    "Mario64Metal",
    "MarioKing",
    "MarioTuxedo",
    "MarioCaptain",
    "MarioUnderwear",
    "MarioHakama",
    "MarioBone",
    "MarioInvisible"
};
// full costume list from 1.3
// attribute otherwise the build log is spammed with unused warnings
// __attribute__((used)) static const char* costumeNames[] = {
//     "Mario", "Mario3D", "Mario64", "Mario64Metal", "MarioAloha", "MarioArmor",
//     // "MarioArmorWestern", // DLC
//     "MarioBandman",
//     // "MarioBatter", // DLC
//     "MarioBone", "MarioCaptain", "MarioClown", "MarioColorClassic", "MarioColorGold", 
//     "MarioColorLuigi", "MarioColorWaluigi", "MarioColorWario",
//     // "MarioConductor", // DLC
//     "MarioCook", "MarioDiddyKong", "MarioDoctor", "MarioDot", "MarioDot3d", "MarioExplorer",
//     "MarioFootball", "MarioGolf", "MarioGunman", "MarioHakama", "MarioHappi",
//     // "MarioHariet", // DLC
//     // "MarioHigh",
//     "MarioKing", "MarioKoopa", "MarioMaker", "MarioMechanic", "MarioNew3DS", "MarioPainter",
//     "MarioPeach", "MarioPilot", "MarioPirate", "MarioPoncho", "MarioPrimitiveMan", "MarioRacer",
//     //"MarioRango", // DLC
//     //"MarioRsv", // DLC
//     "MarioSailor", "MarioSanta",
//     // "MarioSatellite", // DLC
//     "MarioScientist", "MarioShopman", "MarioSnowSuit", "MarioSpaceSuit",
//     // "MarioSpewart", // DLC
//     "MarioSuit",
//     // "MarioSunshine", // DLC
//     "MarioSwimwear",
//     // "MarioTopper", // DLC
//     "MarioTuxedo",
//     // "MarioZombie" // DLC
// };

__attribute__((used)) static const char* stickerNames[] = {
    "StickerCap",
    "StickerWaterfall",
    "StickerSand",
    "StickerLake", 
    "StickerForest",
    "StickerClash",
    "StickerCity",
    "StickerSnow",
    "StickerSea",
    "StickerLava",
    "StickerSky",
    "StickerMoon",
    "StickerPeachDokan",
    "StickerPeachCoin",
    "StickerPeachBlock",
    "StickerPeachBlockQuestion",
    "StickerPeach"
};

__attribute__((used)) static const char* souvenirNames[] = {
    "SouvenirHat1",
    "SouvenirHat2",
    "SouvenirFall1",
    "SouvenirFall2",
    "SouvenirSand1",
    "SouvenirSand2",
    "SouvenirLake1",
    "SouvenirLake2",
    "SouvenirForest1",
    "SouvenirForest2",
    "SouvenirCrash1",
    "SouvenirCrash2",
    "SouvenirCity1",
    "SouvenirCity2", 
    "SouvenirSnow1",
    "SouvenirSnow2",
    "SouvenirSea1", 
    "SouvenirSea2", 
    "SouvenirLava1",
    "SouvenirLava2",
    "SouvenirSky1",
    "SouvenirSky2",
    "SouvenirMoon1",
    "SouvenirMoon2",
    "SouvenirPeach1",
    "SouvenirPeach2"
};

__attribute__((used)) static const char* moonItemNames[] = {
    "MoonCity", // 101
    "MoonForest", // 138
    "MoonWaterfall", //211   
    "MoonCap",  //230
    "MoonLava", // 294
    "MoonSky",  // 360
    "MoonClash",// 398
    "MoonLake", // 430
    "MoonSea",  // 460
    "MoonSand", // 565
    "MoonSnow", // 868
    "MoonPeach", // 933
    "MoonMoon" // 1157
};

__attribute__((used)) static const char* captureListNames[] = {
    "Frog",
    "ElectricWire", // Spark pylon
    "KuriboWing", // Paragoomba
    "Wanwan",   // Chain Chomp
    "WanwanBig", // Big Chain Chomp
    "BreedaWanwan", // Broode's Chain Chomp
    "TRex",
    "Fukankun", // Binoculars
    "Killer", // Bullet Bill
    "Megane", // Moe-eye
    "Cactus", 
    "Kuribo", // Goomba
    "BossKnuckleHand", // Knucklotec's Fist
    "BazookaElectric", // Mini Rocket
    "Kakku", // Glydon
    "JugemFishing", // Lakitu
    "Fastener", // Zipper
    "Pukupuku", // Cheep Cheep
    "GotogotonLake", // Puzzle Part (Lake Kingdom)
    "PackunPoison", // Poison Pirana Plant
    "Senobi", // Uproot
    "FireBros", // Fire Bro
    "Tank", // Sherm
    "Gamane", // Coin Coffer
    "Tree",
    "RockForest", // Boulder
    "FukuwaraiFacePartsKuribo", // Gooma Picture Match Piece
    "Imomu", // Tropical Wiggler
    "Guidepost", // Pole
    "Manhole",
    "Car", // Taxi
    "Radicon", // RC Car
    "Byugo", //Ty-foo
    "Yukimaru", // Shiverian Racer
    "PukupukuSnow", // Cheep Cheep (Snow Kingdom)
    "Hosui", // Gushen
    "Bubble", // Lava Bubble
    "HackFork", // Volbonan
    "HammerBros", // Hammer and Pan Bros
    "CarryMeat", // Meat
    "PackunFire", // Fire Pirana Plant
    "Tsukkun", // Pokio
    "Statue", // Jizo
    "StatueKoopa", // Bowser Statue
    "KaronWing", // Para Bones
    "KillerMagnum", // Bonsai Bill
    "Bull", // Chargin' Chuck
    "Koopa", // Bowser
    "AnagramAlphabetCharacter", // Letter
    "GotogotonCity", // Puzzle Part (Metro Kingdom)
    "FukuwaraiFacePartsMario", // Mario Picture Match Piece
    "Yoshi",

};

__attribute__((used)) static const char* changeStageIdList[] = {};

__attribute__((used)) static const char* changeStageNameList[] = {};

struct stageConnection {
    short fromStageIdIndex;
    short toStageIdIndex;
    short toStageNameIndex;
};

struct shineReplaceText {
    s8 itemType;
    u8 shineItemNameIndex;
    //s8 color;
};

struct shopReplaceText {
    u8 gameIndex;
    u8 slotIndex;
    u8 apItemNameIndex;
    u8 itemClassification;
};

struct HackActorName {
    const char *className;
    const char *hackName;
};

// attribute otherwise the build log is spammed with unused warnings
__attribute__((used)) static HackActorName classHackNames[] = {
    {"SenobiGeneratePoint", "Senobi"},
    {"KuriboPossessed", "Kuribo"},
    {"KillerLauncher", "Killer"},
    {"KillerLauncherMagnum", "KillerMagnum"},
    {"FireBrosPossessed", "FireBros"},
    {"HammerBrosPossessed", "HammerBros"},
    {"ElectricWire", "ElectricWireMover"},
    {"TRexSleep", "TRex"},
    {"TRexPatrol", "TRex"},
    {"WanwanBig", "Wanwan"},  // FIXME: this will make chain chomp captures always be the small
                              // variant for syncing
    {"Koopa","KoopaHack"}
};

struct Transform
{
    sead::Vector3f *position;
    sead::Quatf *rotation;
};

// From Boss Room Unity Example
class VisualUtils
{

public:
    /* 
    * @brief Smoothly interpolates towards the parent transform.
    * @param moveTransform The transform to interpolate
    * @param targetTransform The transform to interpolate towards.
    * @param timeDelta Time in seconds that has elapsed, for purposes of interpolation.
    * @param closingSpeed The closing speed in m/s. This is updated by SmoothMove every time it is called, and will drop to 0 whenever the moveTransform has "caught up". 
    * @param maxAngularSpeed The max angular speed to to rotate at, in degrees/s.
    */
    static float SmoothMove(Transform moveTransform, Transform targetTransform, float timeDelta,
                            float closingSpeed, float maxAngularSpeed);

    constexpr static const float k_MinSmoothSpeed = 0.1f;
    constexpr static const float k_TargetCatchupTime = 0.2f;
};
