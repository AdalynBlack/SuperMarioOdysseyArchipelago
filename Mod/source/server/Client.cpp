#include "server/Client.hpp"
#include "al/layout/SimpleLayoutAppearWaitEnd.h"
#include "al/util/LiveActorUtil.h"
#include "game/SaveData/SaveDataAccessFunction.h"
#include "heap/seadHeapMgr.h"
#include "logger.hpp"
#include "packets/Packet.h"
#include "server/hns/HideAndSeekMode.hpp"

SEAD_SINGLETON_DISPOSER_IMPL(Client)

typedef void (Client::*ClientThreadFunc)(void);

/**
 * @brief Construct a new Client:: Client object
 * 
 * @param bufferSize defines the maximum amount of puppets the client can handle
 */
Client::Client() {

    mHeap = sead::ExpHeap::create(0x50000, "ClientHeap", sead::HeapMgr::instance()->getCurrentHeap(), 8, sead::Heap::cHeapDirection_Forward, false);

    sead::ScopedCurrentHeapSetter heapSetter(
        mHeap);  // every new call after this will use ClientHeap instead of SequenceHeap

    mReadThread = new al::AsyncFunctorThread("ClientReadThread", al::FunctorV0M<Client*, ClientThreadFunc>(this, &Client::readFunc), 0, 0x1000, {0});

    mKeyboard = new Keyboard(nn::swkbd::GetRequiredStringBufferSize());

    mSocket = new SocketClient("SocketClient", mHeap, this);
    
    mPuppetHolder = new PuppetHolder(maxPuppets);

    for (size_t i = 0; i < MAXPUPINDEX; i++)
    {
        mPuppetInfoArr[i] = new PuppetInfo();

        sprintf(mPuppetInfoArr[i]->puppetName, "Puppet%zu", i);
    }

    strcpy(mDebugPuppetInfo.puppetName, "PuppetDebug");

    mConnectCount = 0;

    curCollectedShines.fill(-1);

    collectedShineCount = 0;

    mShineArray.allocBuffer(100, nullptr); // max of 100 shine actors in buffer

    nn::account::GetLastOpenedUser(&mUserID);

    nn::account::Nickname playerName;
    nn::account::GetNickname(&playerName, mUserID);
    Logger::setLogName(playerName.name);  // set Debug logger name to player name

    mUsername = playerName.name;

    apChatLineCount = 0;
    
    worldScenarios.fill(1);
    worldPayCounts.fill(-1);

    collectedShines.fill(0);
    collectedOutfits.fill(0);
    collectedStickers.fill(0);
    collectedSouvenirs.fill(0);
    collectedCaptures.fill(0);
    checkedCaptures.fill(0);

    shineTextReplacements.fill({0, 0});
    shineItemNames.fill(sead::FixedSafeString<40>());
    shineColors.fill(0);

    shopCapTextReplacements.fill({254, 255, 255, 255});
    shopClothTextReplacements.fill({254, 255, 255, 255});
    shopStickerTextReplacements.fill({254, 255, 255, 255});
    shopGiftTextReplacements.fill({254, 255, 255, 255});
    shopMoonTextReplacements.fill({254, 255, 255, 255});

    apGameNames.fill(sead::WFixedSafeString<40>());
    apSlotNames.fill(sead::WFixedSafeString<40>());
    apItemNames.fill(sead::WFixedSafeString<40>());

    mUserID.print();

    Logger::log("Player Name: %s\n", playerName.name);

    Logger::log("%s Build Number: %s\n", playerName.name, TOSTRING(BUILDVERSTR));

}

/**
 * @brief initializes client class using initInfo obtained from StageScene::init
 * 
 * @param initInfo init info used to create layouts used by client
 */
void Client::init(al::LayoutInitInfo const &initInfo, GameDataHolderAccessor holder) {

    mConnectStatus = new (mHeap) al::SimpleLayoutAppearWaitEnd("", "SaveMessage", initInfo, 0, false);
    al::setPaneString(mConnectStatus, "TxtSave", u"Connecting to Server.", 0);
    al::setPaneString(mConnectStatus, "TxtSaveSh", u"Connecting to Server.", 0);

    mUIMessage = new (mHeap) al::WindowConfirmWait("ServerWaitConnect", "WindowConfirmWait", initInfo);
    mUIMessage->setTxtMessage(u"a");
    mUIMessage->setTxtMessageConfirm(u"b");

    mHolder = holder;

    startThread();

    Logger::log("Heap Free Size: %f/%f\n", mHeap->getFreeSize() * 0.001f, mHeap->getSize() * 0.001f);
}

/**
 * @brief starts client read thread
 * 
 * @return true if read thread was succesfully started
 * @return false if read thread was unable to start, or thread was already started.
 */
bool Client::startThread() {
    if(mReadThread->isDone() ) {
        mReadThread->start();
        Logger::log("Read Thread Successfully Started.\n");
        return true;
    }else {
        Logger::log("Read Thread has already started! Or other unknown reason.\n");
        return false;
    }
}

/**
 * @brief starts a connection using client's TCP socket class, pulling up the software keyboard for user inputted IP if save file does not have one saved.
 * 
 * @return true if successful connection to server
 * @return false if connection was unable to establish
 */
bool Client::startConnection() {

    bool isNeedSave = false;

    bool isOverride = al::isPadHoldZL(-1);

    if (mServerIP.isEmpty() || isOverride) {
        mKeyboard->setHeaderText(u"Save File does not contain an IP!");
        mKeyboard->setSubText(u"Please set a Server IP Below.");
        mServerIP = "127.0.0.1";
        Client::openKeyboardIP();
        isNeedSave = true;
    }

    if (!mServerPort || isOverride) {
        mKeyboard->setHeaderText(u"Save File does not contain a port!");
        mKeyboard->setSubText(u"Please set a Server Port Below.");
        mServerPort = 1027;
        Client::openKeyboardPort();
        isNeedSave = true;
    }

    if (isNeedSave) {
        SaveDataAccessFunction::startSaveDataWrite(mHolder.mData);
    }

    // This might need to be changed later
    // Repeat connection attempts until successful
    while (!mIsConnectionActive) {
        mIsConnectionActive = mSocket->init(mServerIP.cstr(), mServerPort).isSuccess();
        nn::os::YieldThread();
        nn::os::SleepThread(nn::TimeSpan::FromNanoSeconds(2500000000));
    }

    if (mIsConnectionActive) {

        Logger::log("Succesful Connection. Waiting to receive init packet.\n");

        bool waitingForInitPacket = true;
        // wait for client init packet

        while (waitingForInitPacket) {

            Packet *curPacket = mSocket->tryGetPacket();

            if (curPacket) {
                
                if (curPacket->mType == PacketType::CLIENTINIT) {
                    InitPacket* initPacket = (InitPacket*)curPacket;

                    Logger::log("Server Max Player Size: %d\n", initPacket->maxPlayers);

                    maxPuppets = initPacket->maxPlayers - 1;

                    waitingForInitPacket = false;
                }

                mHeap->free(curPacket);

            } else {
                Logger::log("Receive failed! Stopping Connection.\n");
                mIsConnectionActive = false;
                waitingForInitPacket = false;
            }
        }


    }

    return mIsConnectionActive;
}

/**
 * @brief Opens up OS's software keyboard in order to change the currently used server IP.
 * @returns whether or not a new IP has been defined and needs to be saved.
 */
bool Client::openKeyboardIP() {

    if (!sInstance) {
        Logger::log("Static Instance is null!\n");
        return false;
    }

    // opens swkbd with the initial text set to the last saved IP
    sInstance->mKeyboard->openKeyboard(
        sInstance->mServerIP.cstr(), [](nn::swkbd::KeyboardConfig& config) {
            config.keyboardMode = nn::swkbd::KeyboardMode::ModeASCII;
            config.textMaxLength = MAX_HOSTNAME_LENGTH;
            config.textMinLength = 1;
            config.isUseUtf8 = true;
            config.inputFormMode = nn::swkbd::InputFormMode::OneLine;
        });

    hostname prevIp = sInstance->mServerIP;

    while (true) {
        if (sInstance->mKeyboard->isThreadDone()) {
            if(!sInstance->mKeyboard->isKeyboardCancelled())
                sInstance->mServerIP = sInstance->mKeyboard->getResult();
            break;
        }
        nn::os::YieldThread(); // allow other threads to run
    }

    bool isFirstConnect = prevIp != sInstance->mServerIP;

    sInstance->mSocket->setIsFirstConn(isFirstConnect);

    return isFirstConnect;
}

/**
 * @brief Opens up OS's software keyboard in order to change the currently used server port.
 * @returns whether or not a new port has been defined and needs to be saved.
 */
bool Client::openKeyboardPort() {

    if (!sInstance) {
        Logger::log("Static Instance is null!\n");
        return false;
    }

    // opens swkbd with the initial text set to the last saved port
    char buf[6];
    nn::util::SNPrintf(buf, 6, "%u", sInstance->mServerPort);

    sInstance->mKeyboard->openKeyboard(buf, [](nn::swkbd::KeyboardConfig& config) {
        config.keyboardMode = nn::swkbd::KeyboardMode::ModeNumeric;
        config.textMaxLength = 5;
        config.textMinLength = 2;
        config.isUseUtf8 = true;
        config.inputFormMode = nn::swkbd::InputFormMode::OneLine;
    });

    int prevPort = sInstance->mServerPort;

    while (true) {
        if (sInstance->mKeyboard->isThreadDone()) {
            if(!sInstance->mKeyboard->isKeyboardCancelled())
                sInstance->mServerPort = ::atoi(sInstance->mKeyboard->getResult());
            break;
        }
        nn::os::YieldThread(); // allow other threads to run
    }

    bool isFirstConnect = prevPort != sInstance->mServerPort;

    sInstance->mSocket->setIsFirstConn(isFirstConnect);

    return isFirstConnect;
}


void Client::showUIMessage(const char16_t* msg) {
    if (!sInstance) {
        return;
    }

    sInstance->mUIMessage->setTxtMessageConfirm(msg);

    al::hidePane(sInstance->mUIMessage, "Page01");  // hide A button prompt

    if (!sInstance->mUIMessage->mIsAlive) {
        sInstance->mUIMessage->appear();

        sInstance->mUIMessage->playLoop();
    }

    al::startAction(sInstance->mUIMessage, "Confirm", "State");
}

void Client::hideUIMessage() {
    if (!sInstance) {
        return;
    }

    sInstance->mUIMessage->tryEnd();
}

/**
 * @brief main thread function for read thread, responsible for processing packets from server
 * 
 */
void Client::readFunc() {

    if (waitForGameInit) {
        nn::os::YieldThread(); // sleep the thread for the first thing we do so that game init can finish
        nn::os::SleepThread(nn::TimeSpan::FromSeconds(2));
        waitForGameInit = false;
    }

    mConnectStatus->appear();

    al::startAction(mConnectStatus, "Loop", "Loop");

    if (!startConnection()) {

        Logger::log("Failed to Connect to Server.\n");

        nn::os::SleepThread(nn::TimeSpan::FromNanoSeconds(250000000)); // sleep active thread for 0.25 seconds

        mConnectStatus->end();
                
        return;
    }

    nn::os::SleepThread(nn::TimeSpan::FromNanoSeconds(500000000)); // sleep for 0.5 seconds to let connection layout fully show (probably should find a better way to do this)

    mConnectStatus->end();

    while(mIsConnectionActive) {

        Packet *curPacket = mSocket->tryGetPacket();  // will block until a packet has been received, or socket disconnected

        if (curPacket) {

            switch (curPacket->mType)
            {
            case PacketType::PLAYERINF:
                updatePlayerInfo((PlayerInf*)curPacket);
                break;
            case PacketType::GAMEINF:
                updateGameInfo((GameInf*)curPacket);
                break;
            case PacketType::HACKCAPINF:
                updateHackCapInfo((HackCapInf *)curPacket);
                break;
            case PacketType::CAPTUREINF:                    
                updateCaptureInfo((CaptureInf*)curPacket);
                break;
            case PacketType::PLAYERCON:
                updatePlayerConnect((PlayerConnect*)curPacket);

                // Send relevant info packets when another client is connected

                if (lastGameInfPacket != emptyGameInfPacket) {
                    // Assume game packets are empty from first connection
                    if (lastGameInfPacket.mUserID != mUserID)
                        lastGameInfPacket.mUserID = mUserID;
                    mSocket->send(&lastGameInfPacket);
                }

                // No need to send player/costume packets if they're empty
                if (lastPlayerInfPacket.mUserID == mUserID)
                    mSocket->send(&lastPlayerInfPacket);
                if (lastCostumeInfPacket.mUserID == mUserID)
                    mSocket->send(&lastCostumeInfPacket);
                if (lastTagInfPacket.mUserID == mUserID)
                    mSocket->send(&lastTagInfPacket);
                if (lastCaptureInfPacket.mUserID == mUserID)
                    mSocket->send(&lastCaptureInfPacket);

                break;
            case PacketType::COSTUMEINF:
                updateCostumeInfo((CostumeInf*)curPacket);
                break;
            case PacketType::CHECK:
                receiveCheck((Check*)curPacket);
                break;
            case PacketType::SHINECHECKS:
                updateSentShines((ShineChecks*)curPacket);
                break;
            case PacketType::APCHATMESSAGE:
                updateChatMessages((ArchipelagoChatMessage*)curPacket);
                break;
            case PacketType::SLOTDATA:
                updateSlotData((SlotData*)curPacket);
                break;
            case PacketType::APINFO:
                addApInfo((ApInfo*)curPacket);
                break;
            case PacketType::SHINEREPLACE:
                updateShineReplace((ShineReplacePacket*)curPacket);
                break;
            case PacketType::SHINECOLOR:
                updateShineColor((ShineColor*)curPacket);
                break;
            case PacketType::SHOPREPLACE:
                updateShopReplace((ShopReplacePacket*)curPacket);
                break;
            case PacketType::UNLOCKWORLD:
                updateWorlds((UnlockWorld*)curPacket);
                break;
            case PacketType::DEATHLINK:
                receiveDeath((Deathlink*)curPacket);
                break;
            case PacketType::PLAYERDC:
                Logger::log("Received Player Disconnect!\n");
                curPacket->mUserID.print();
                disconnectPlayer((PlayerDC*)curPacket);
                break;
            case PacketType::TAGINF:
                updateTagInfo((TagInf*)curPacket);
                break;
            case PacketType::CHANGESTAGE:
                sendToStage((ChangeStagePacket*)curPacket);
                break;
            case PacketType::CLIENTINIT: {
                InitPacket* initPacket = (InitPacket*)curPacket;
                Logger::log("Server Max Player Size: %d\n", initPacket->maxPlayers);
                maxPuppets = initPacket->maxPlayers - 1;
                break;
            }
			case PacketType::UDPINIT: {
				UdpInit* initPacket = (UdpInit*)curPacket;
				Logger::log("Received udp init packet from server\n");
				
				sInstance->mSocket->setPeerUdpPort(initPacket->port);
				sendUdpHolePunch();
				sendUdpInit();
				
				break;
			}
			case PacketType::HOLEPUNCH: 
				sendUdpHolePunch();
				break;
            default:
                Logger::log("Discarding Unknown Packet Type.\n");
                break;
            }

            mHeap->free(curPacket);

        }else { // if false, socket has errored or disconnected, so restart the connection
            Logger::log("Client Socket Encountered an Error, restarting connection! Errno: 0x%x\n", mSocket->socket_errno);
        }

    }

    Logger::log("Client Read Thread ending.\n");
}

/**
 * @brief sends player info packet to current server
 * 
 * @param player pointer to current player class, used to get translation, animation, and capture data
 */
void Client::sendPlayerInfPacket(const PlayerActorBase *playerBase, bool isYukimaru) {

    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }
    
    if(!playerBase) {
        Logger::log("Error: Null Player Reference\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);

    PlayerInf *packet = new PlayerInf();
    packet->mUserID = sInstance->mUserID;

    packet->playerPos = al::getTrans(playerBase);

    al::calcQuat(&packet->playerRot,
                 playerBase);  // calculate rotation based off pose instead of using quat rotation

    if (!isYukimaru) { 
        
        PlayerActorHakoniwa* player = (PlayerActorHakoniwa*)playerBase;

        for (size_t i = 0; i < 6; i++)
        {
            packet->animBlendWeights[i] = player->mPlayerAnimator->getBlendWeight(i);
        }

        const char *hackName = player->mHackKeeper->getCurrentHackName();

        if (hackName != nullptr) {

            sInstance->isClientCaptured = true;

            const char* actName = al::getActionName(player->mHackKeeper->currentHackActor);

            if (actName) {
                packet->actName = PlayerAnims::FindType(actName);
                packet->subActName = PlayerAnims::Type::Unknown;
                //strcpy(packet.actName, actName); 
            } else {
                packet->actName = PlayerAnims::Type::Unknown;
                packet->subActName = PlayerAnims::Type::Unknown;
            }
        } else {
            packet->actName = PlayerAnims::FindType(player->mPlayerAnimator->mAnimFrameCtrl->getActionName());
            packet->subActName = PlayerAnims::FindType(player->mPlayerAnimator->curSubAnim.cstr());

            sInstance->isClientCaptured = false;
        }

    } else {

        // TODO: implement YukimaruRacePlayer syncing
        
        for (size_t i = 0; i < 6; i++)
        {
            packet->animBlendWeights[i] = 0;
        }

        sInstance->isClientCaptured = false;

        packet->actName = PlayerAnims::Type::Unknown;
        packet->subActName = PlayerAnims::Type::Unknown;
    }
    
    if(sInstance->lastPlayerInfPacket != *packet) {
        sInstance->lastPlayerInfPacket = *packet; // deref packet and store in client memory
        sInstance->mSocket->queuePacket(packet);
    } else {
        sInstance->mHeap->free(packet); // free packet if we're not using it
    }

}

/**
 * @brief sends info related to player's cap actor to server
 * 
 * @param hackCap pointer to cap actor, used to get translation, animation, and state info
 */
void Client::sendHackCapInfPacket(const HackCap* hackCap) {

    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);
    
    bool isFlying = hackCap->isFlying();

    // if cap is in flying state, send packet as often as this function is called
    if (isFlying) {
        HackCapInf *packet = new HackCapInf();
        packet->mUserID = sInstance->mUserID;
        packet->capPos = al::getTrans(hackCap);

        packet->isCapVisible = isFlying;

        packet->capQuat.x = hackCap->mJointKeeper->mJointRot.x;
        packet->capQuat.y = hackCap->mJointKeeper->mJointRot.y;
        packet->capQuat.z = hackCap->mJointKeeper->mJointRot.z;
        packet->capQuat.w = hackCap->mJointKeeper->mSkew;

        strcpy(packet->capAnim, al::getActionName(hackCap));

        sInstance->mSocket->queuePacket(packet);

        sInstance->isSentHackInf = true;

    } else if (sInstance->isSentHackInf) { // if cap is not flying, check to see if previous function call sent a packet, and if so, send one final packet resetting cap data.
        HackCapInf *packet = new HackCapInf();
        packet->mUserID = sInstance->mUserID;
        packet->isCapVisible = false;
        packet->capPos = sead::Vector3f::zero;
        packet->capQuat = sead::Quatf::unit;
        sInstance->mSocket->queuePacket(packet);
        sInstance->isSentHackInf = false;
    }
}

/**
 * @brief 
 * Sends both stage info and player 2D info to the server.
 * @param player 
 * @param holder 
 */
void Client::sendGameInfPacket(const PlayerActorHakoniwa* player, GameDataHolderAccessor holder) {

    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);
    
    GameInf *packet = new GameInf();
    packet->mUserID = sInstance->mUserID;

    if (player) {
        packet->is2D = player->mDimKeeper->is2DModel;
    } else {
        packet->is2D = false;
    }

    packet->scenarioNo = holder.mData->mGameDataFile->getScenarioNo();

    strcpy(packet->stageName, GameDataFunction::getCurrentStageName(holder));

    if (*packet != sInstance->lastGameInfPacket && *packet != sInstance->emptyGameInfPacket) {
        sInstance->lastGameInfPacket = *packet;
        sInstance->mSocket->queuePacket(packet);
    } else {
        sInstance->mHeap->free(packet); // free packet if we're not using it
    }
}

/**
 * @brief 
 * Sends only stage info to the server.
 * @param holder 
 */
void Client::sendGameInfPacket(GameDataHolderAccessor holder) {

    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);
    
    GameInf *packet = new GameInf();
    packet->mUserID = sInstance->mUserID;

    packet->is2D = false;

    packet->scenarioNo = holder.mData->mGameDataFile->getScenarioNo();

    strcpy(packet->stageName, GameDataFunction::getCurrentStageName(holder));

    if (*packet != sInstance->emptyGameInfPacket) {
        sInstance->lastGameInfPacket = *packet;
        sInstance->mSocket->queuePacket(packet);
    }
}

/**
 * @brief 
 * 
 */
void Client::sendTagInfPacket() {

    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);

    HideAndSeekMode* hsMode = GameModeManager::instance()->getMode<HideAndSeekMode>();

    if (!GameModeManager::instance()->isMode(GameMode::HIDEANDSEEK)) {
        Logger::log("State is not Hide and Seek!\n");
        return;
    }

    HideAndSeekInfo* curInfo = GameModeManager::instance()->getInfo<HideAndSeekInfo>();

    TagInf *packet = new TagInf();

    packet->mUserID = sInstance->mUserID;

    packet->isIt = hsMode->isPlayerIt() && hsMode->isModeActive();

    packet->minutes = curInfo->mHidingTime.mMinutes;
    packet->seconds = curInfo->mHidingTime.mSeconds;
    packet->updateType = static_cast<TagUpdateType>(TagUpdateType::STATE | TagUpdateType::TIME);

    sInstance->mSocket->queuePacket(packet);

    sInstance->lastTagInfPacket = *packet;
}

/**
 * @brief 
 * 
 * @param body 
 * @param cap 
 */
void Client::sendCostumeInfPacket(const char* body, const char* cap) {

    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    if (!strcmp(body, "") && !strcmp(cap, "")) { return; }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);
    
    CostumeInf *packet = new CostumeInf(body, cap);
    packet->mUserID = sInstance->mUserID;
    sInstance->lastCostumeInfPacket = *packet;
    sInstance->mSocket->queuePacket(packet);
}

/**
 * @brief 
 * 
 * @param player 
 */
void Client::sendCaptureInfPacket(const PlayerActorHakoniwa* player) {

    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);
    
    if (sInstance->isClientCaptured && !sInstance->isSentCaptureInf) {
        CaptureInf *packet = new CaptureInf();
        packet->mUserID = sInstance->mUserID;
        strcpy(packet->hackName, tryConvertName(player->mHackKeeper->getCurrentHackName()));
        sInstance->mSocket->queuePacket(packet);
        sInstance->lastCaptureInfPacket = *packet;
        sInstance->isSentCaptureInf = true;
    } else if (!sInstance->isClientCaptured && sInstance->isSentCaptureInf) {
        CaptureInf *packet = new CaptureInf();
        packet->mUserID = sInstance->mUserID;
        strcpy(packet->hackName, "");
        sInstance->mSocket->queuePacket(packet);
        sInstance->lastCaptureInfPacket = *packet;
        sInstance->isSentCaptureInf = false;
    }
}

/**
 * @brief
 */
void Client::resendInitPackets() {
    // CostumeInfPacket
    if (lastCostumeInfPacket.mUserID == mUserID) {
        mSocket->queuePacket(&lastCostumeInfPacket);
    }

    // GameInfPacket
    if (lastGameInfPacket != emptyGameInfPacket) {
        mSocket->queuePacket(&lastGameInfPacket);
    }

    // TagInfPacket
    if (lastTagInfPacket.mUserID == mUserID) {
        mSocket->queuePacket(&lastTagInfPacket);
    }

    // CaptureInfPacket
    if (lastCaptureInfPacket.mUserID == mUserID) {
        mSocket->queuePacket(&lastCaptureInfPacket);
    }
}

/**
 * @brief
 *
 * @param itemName
 */
void Client::sendDeathlinkPacket() {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);

    Deathlink* packet = new Deathlink();
    packet->mUserID = sInstance->mUserID;

    sInstance->mSocket->queuePacket(packet);
}

void Client::receiveDeath(Deathlink* packet)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sInstance->apDeath = true;
    sInstance->dying = true;

}

void Client::setDying(bool value)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sInstance->dying = value;
}

void Client::setApDeath(bool value)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sInstance->apDeath = value;
}

/**
 * @brief 
 * 
 * @param packet 
 */
void Client::updatePlayerInfo(PlayerInf *packet) {

    PuppetInfo* curInfo = findPuppetInfo(packet->mUserID, false);

    if (!curInfo) {
        return;
    }

    if(!curInfo->isConnected) {
        curInfo->isConnected = true;
    }

    curInfo->playerPos = packet->playerPos;

    // check if rotation is larger than zero and less than or equal to 1
    if(abs(packet->playerRot.x) > 0.f || abs(packet->playerRot.y) > 0.f || abs(packet->playerRot.z) > 0.f || abs(packet->playerRot.w) > 0.f) {
        if(abs(packet->playerRot.x) <= 1.f || abs(packet->playerRot.y) <= 1.f || abs(packet->playerRot.z) <= 1.f || abs(packet->playerRot.w) <= 1.f) {
            curInfo->playerRot = packet->playerRot;
        }
    }

        if (packet->actName != PlayerAnims::Type::Unknown) {
            strcpy(curInfo->curAnimStr, PlayerAnims::FindStr(packet->actName));
            if (curInfo->curAnimStr[0] == '\0')
                Logger::log("[ERROR] %s: actName was out of bounds: %d\n", __func__, packet->actName);
        } else {
            strcpy(curInfo->curAnimStr, "Wait");
        }

        if(packet->subActName != PlayerAnims::Type::Unknown) {
            strcpy(curInfo->curSubAnimStr, PlayerAnims::FindStr(packet->subActName));
            if (curInfo->curSubAnimStr[0] == '\0')
                Logger::log("[ERROR] %s: subActName was out of bounds: %d\n", __func__, packet->subActName);
        } else {
            strcpy(curInfo->curSubAnimStr, "");
        }

    curInfo->curAnim = packet->actName;
    curInfo->curSubAnim = packet->subActName;

    for (size_t i = 0; i < 6; i++)
    {
        // weights can only be between 0 and 1
        if(packet->animBlendWeights[i] >= 0.f && packet->animBlendWeights[i] <= 1.f) {
            curInfo->blendWeights[i] = packet->animBlendWeights[i];
        }
    }

    //TEMP

    if(!curInfo->isCapThrow) {
        curInfo->capPos = packet->playerPos;
    }

}

/**
 * @brief 
 * 
 * @param packet 
 */
void Client::updateHackCapInfo(HackCapInf *packet) {

    PuppetInfo* curInfo = findPuppetInfo(packet->mUserID, false);

    if (curInfo) {
        curInfo->capPos = packet->capPos;
        curInfo->capRot = packet->capQuat;

        curInfo->isCapThrow = packet->isCapVisible;

        strcpy(curInfo->capAnim, packet->capAnim);
    }
}

/**
 * @brief 
 * 
 * @param packet 
 */
void Client::updateCaptureInfo(CaptureInf* packet) {
    
    PuppetInfo* curInfo = findPuppetInfo(packet->mUserID, false);
        
    if (!curInfo) {
        return;
    }

    curInfo->isCaptured = strlen(packet->hackName) > 0;

    if (curInfo->isCaptured) {
        strcpy(curInfo->curHack, packet->hackName);
    }
}

/**
 * @brief 
 * 
 * @param packet 
 */
void Client::updateCostumeInfo(CostumeInf *packet) {

    PuppetInfo* curInfo = findPuppetInfo(packet->mUserID, false);

    if (!curInfo) {
        return;
    }

    strcpy(curInfo->costumeBody, packet->bodyModel);
    strcpy(curInfo->costumeHead, packet->capModel);
}

/**
 * @brief 
 * 
 * @param packet 
 */
//void Client::updateShineInfo(ShineCollect* packet) {
//    if(collectedShineCount < curCollectedShines.size() - 1) {
//        curCollectedShines[collectedShineCount] = packet->shineId;
//        collectedShineCount++;
//        
//    }
//}

/**
 * @brief 
 * 
 * @param packet 
 */
void Client::updatePlayerConnect(PlayerConnect* packet) {
    
    PuppetInfo* curInfo = findPuppetInfo(packet->mUserID, true);

    if (!curInfo) {
        return;
    }

    if (curInfo->isConnected) {

        Logger::log("Info is already being used by another connected player!\n");
        packet->mUserID.print("Connection ID");
        curInfo->playerID.print("Target Info");

    } else {

        packet->mUserID.print("Player Connected! ID");

        curInfo->playerID = packet->mUserID;
        curInfo->isConnected = true;
        strcpy(curInfo->puppetName, packet->clientName);

        mConnectCount++;
    }
}

/**
 * @brief 
 * 
 * @param packet 
 */
void Client::updateGameInfo(GameInf *packet) {

    PuppetInfo* curInfo = findPuppetInfo(packet->mUserID, false);

    if (!curInfo) {
        return;
    }

    if(curInfo->isConnected) {

        curInfo->scenarioNo = packet->scenarioNo;

        if(strcmp(packet->stageName, "") != 0 && strlen(packet->stageName) > 3) {
            strcpy(curInfo->stageName, packet->stageName);
        }

        curInfo->is2D = packet->is2D;
    }
}

/**
 * @brief 
 * 
 * @param packet 
 */
void Client::updateTagInfo(TagInf *packet) {
    
    // if the packet is for our player, edit info for our player
    if (packet->mUserID == mUserID && GameModeManager::instance()->isMode(GameMode::HIDEANDSEEK)) {

        HideAndSeekMode* mMode = GameModeManager::instance()->getMode<HideAndSeekMode>();
        HideAndSeekInfo* curInfo = GameModeManager::instance()->getInfo<HideAndSeekInfo>();

        if (packet->updateType & TagUpdateType::STATE) {
            mMode->setPlayerTagState(packet->isIt);
        }

        if (packet->updateType & TagUpdateType::TIME) {
            curInfo->mHidingTime.mSeconds = packet->seconds;
            curInfo->mHidingTime.mMinutes = packet->minutes;
        }

        return;

    }

    PuppetInfo* curInfo = findPuppetInfo(packet->mUserID, false);

    if (!curInfo) {
        return;
    }

    curInfo->isIt = packet->isIt;
    curInfo->seconds = packet->seconds;
    curInfo->minutes = packet->minutes;
}

/**
 * @brief 
 * 
 * @param packet 
 */
void Client::sendToStage(ChangeStagePacket* packet) {
    if (mSceneInfo && mSceneInfo->mSceneObjHolder) {

        GameDataHolderAccessor accessor(mSceneInfo->mSceneObjHolder);

        if (packet->scenarioNo > 0) {
            setScenario(accessor.mData->mWorldList->tryFindWorldIndexByStageName(packet->changeStage), packet->scenarioNo);
        }

        Logger::log("Sending Player to %s at Entrance %s in Scenario %d\n", packet->changeStage,
                     packet->changeID, packet->scenarioNo);
        
        ChangeStageInfo info(accessor.mData, packet->changeID, packet->changeStage, false, packet->scenarioNo, static_cast<ChangeStageInfo::SubScenarioType>(packet->subScenarioType));
        GameDataFunction::tryChangeNextStage(accessor, &info);
    }
}
/**
 * @brief 
 * Send a udp holepunch packet to the server
 */
void Client::sendUdpHolePunch() {

    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);
    
    HolePunch *packet = new HolePunch();
	
    packet->mUserID = sInstance->mUserID;

    sInstance->mSocket->queuePacket(packet);
}
/**
 * @brief 
 * Send a udp init packet to server
 */
void Client::sendUdpInit() {

    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);
    
    UdpInit *packet = new UdpInit();
	
    packet->mUserID = sInstance->mUserID;
	packet->port = sInstance->mSocket->getLocalUdpPort();
	
    sInstance->mSocket->queuePacket(packet);
}
/**
 * @brief 
 * 
 * @param packet 
 */
void Client::disconnectPlayer(PlayerDC *packet) {

    PuppetInfo* curInfo = findPuppetInfo(packet->mUserID, false);

    if (!curInfo || !curInfo->isConnected) {
        return;
    }
    
    curInfo->isConnected = false;

    curInfo->scenarioNo = -1;
    strcpy(curInfo->stageName, "");
    curInfo->isInSameStage = false;

    mConnectCount--;
}

/**
 * @brief 
 * 
 * @param shineId 
 * @return true 
 * @return false 
 */
bool Client::isShineCollected(int shineId) {

    for (size_t i = 0; i < curCollectedShines.size(); i++)
    {
        if(curCollectedShines[i] >= 0) {
            if(curCollectedShines[i] == shineId) {
                return true;
            }
        }
    }
    
    return false;

}

/**
 * @brief 
 * 
 * @param id 
 * @return int 
 */
PuppetInfo* Client::findPuppetInfo(const nn::account::Uid& id, bool isFindAvailable) {

    PuppetInfo *firstAvailable = nullptr;

    for (size_t i = 0; i < getMaxPlayerCount() - 1; i++) {

        PuppetInfo* curInfo = mPuppetInfoArr[i];

        if (curInfo->playerID == id) {
            return curInfo;
        } else if (isFindAvailable && !firstAvailable && !curInfo->isConnected) {
            firstAvailable = curInfo;
        }
    }

    if (!firstAvailable) {
        Logger::log("Unable to find Assigned Puppet for Player!\n");
        id.print("User ID");
    }

    return firstAvailable;
}

/**
 * @brief 
 * 
 * @param holder 
 */
void Client::setStageInfo(GameDataHolderAccessor holder) {
    if (sInstance) {

        sInstance->mStageName = GameDataFunction::getCurrentStageName(holder);
        sInstance->mScenario = holder.mData->mGameDataFile->getScenarioNo(); //holder.mData->mGameDataFile->getMainScenarioNoCurrent();
        
        sInstance->mPuppetHolder->setStageInfo(sInstance->mStageName.cstr(), sInstance->mScenario);
    }
}

void Client::sendStage(GameDataHolderWriter writer, const ChangeStageInfo* stageInfo) {
    if (sInstance)
    {
        GameDataHolderAccessor accessor(sInstance->mCurStageScene);

        setScenario(stageInfo->changeStageName.cstr(), stageInfo->scenarioNo);
        //setMessage(1, "onGrandShineStageChange");
        //setMessage(2, stageInfo->changeStageName.cstr());

        if (GameDataFunction::getWorldIndexWaterfall() ==
            GameDataFunction::getCurrentWorldId(accessor)
                 || GameDataFunction::isUnlockedCurrentWorld(accessor)) {
            GameDataFunction::tryChangeNextStage(accessor, stageInfo);
        } else {
            int i = 0;
            for (i = GameDataFunction::getWorldIndexSpecial2(); i > 0; i--)
            {
                if (GameDataFunction::isUnlockedWorld(accessor, i))
                {
                    break;
                }
            }
            ChangeStageInfo info(accessor.mData, "",
                                 GameDataFunction::getMainStageName(accessor, i), false, -1,
                static_cast<ChangeStageInfo::SubScenarioType>(0));
            GameDataFunction::tryChangeNextStage(accessor, &info);
        }
    }
}

void Client::sendChangeStagePacket(GameDataHolderAccessor accessor) {
    if (!sInstance) {
        Logger::log("Client Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);

    ChangeStagePacket* packet = new ChangeStagePacket();
    int worldId = accessor.mData->mWorldList->tryFindWorldIndexByStageName(GameDataFunction::getCurrentStageName(accessor));
    strcpy(packet->changeStage, GameDataFunction::getMainStageName(accessor, worldId));

    sInstance->mSocket->queuePacket(packet);
}

void Client::setScenario(int worldID, int scenario)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sInstance->worldScenarios[worldID] = scenario;

}

bool Client::setScenario(const char* worldName, int scenario) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return false;
    }

    GameDataHolderAccessor accessor(sInstance->mCurStageScene);

    int worldID = accessor.mData->mWorldList->tryFindWorldIndexByStageName(worldName);
    if (scenario == -1)
    {
        //setMessage(3, "ChangeStageInfo failed to init");
    }

    // Exclude revisitable scenarios like festival
    if (!(al::isEqualString(worldName, "CityWorldHomeStage") && scenario == 3)) {
        if (scenario != getScenario(worldID) &&
            scenario <= accessor.mData->mWorldList->getMoonRockScenarioNo(worldID) &&
            !GameDataFunction::isUnlockedWorld(accessor, worldID)) {
            if (getScenario(worldID) < scenario) {
                //setMessage(1, "Scenario Updated");
                setScenario(worldID, scenario);
            }
            return true;
        }
    }
    return false;
}

int Client::getScenario(const char* worldName)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return -1;
    }

    GameDataHolderAccessor accessor(sInstance->mCurStageScene);

    int worldID = accessor.mData->mWorldList->tryFindWorldIndexByStageName(worldName);

    /*if (sInstance->worldScenarios[worldID] < GameDataFunction::getWorldScenarioNo(accessor, worldID))
    {
        setScenario(worldID, GameDataFunction::getWorldScenarioNo(accessor, worldID));
    }*/
    return sInstance->worldScenarios[worldID];

}

int Client::getScenario(int worldID)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return -1;
    }

    return sInstance->worldScenarios[worldID];
}

void Client::sendCorrectScenario(const ChangeStageInfo* stageInfo)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    GameDataHolderAccessor accessor(sInstance->mCurStageScene);
    // try changing isReturn (param_4)
    /*if (stageInfo->isReturn)
    {
        setMessage(1, "isReturn: True");
        
    } else {
        setMessage(1, "isReturn: False");
    }
    sead::FixedSafeString<40> str;
    str = "";
    str.append("subScenario type: ");
    str.append(static_cast<char>(48 + static_cast<unsigned int>(stageInfo->subType)));
    setMessage(2, str.cstr());*/
    ChangeStageInfo info(accessor.mData, stageInfo->changeStageId.cstr(),
                         stageInfo->changeStageName.cstr(),
                         false,
                         getScenario(stageInfo->changeStageName.cstr()),
                         static_cast<ChangeStageInfo::SubScenarioType>(0));
    GameDataFunction::tryChangeNextStage(accessor, &info);
}

void Client::setCheckIndex(int index)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sInstance->checkIndex = index;
}

void Client::sendCheckPacket(int locationId, int itemType) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);

    Check *packet = new Check();
    packet->locationId = locationId;
    packet->itemType = itemType;

    sInstance->mSocket->queuePacket(packet);
}

void Client::sendCheckPacket(int itemType, const char* objId, const char* stageName) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sead::ScopedCurrentHeapSetter setter(sInstance->mHeap);

    Check *packet = new Check();
    packet->itemType = itemType;
    strcpy(packet->objId, objId);
    strcpy(packet->stage, stageName);

    sInstance->mSocket->queuePacket(packet);
}

void Client::receiveCheck(Check* packet)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    int itemType = packet->itemType;

    struct ShopItem::ShopItemInfo amiiboData = {1, 1};
    struct ShopItem::ShopItemInfo* amiibo = &amiiboData;
    struct ShopItem::ItemInfo info = {1, {}, static_cast<ShopItem::ItemType>(itemType), 1, amiibo, true};

    struct ShopItem::ItemInfo* infoPtr;
    GameDataHolderAccessor accessor(sInstance->mCurStageScene);
    bool updateIndex = false;
    sead::FixedSafeString<40> indexMessage;
    indexMessage = "";
    indexMessage.append("Received item index ");
    int index = packet->index;
    int trim = index;
    if (index >= 1000)
    {
        indexMessage.append(static_cast<char>(48 + trim / 1000));
        trim = trim % 1000;
    }
    if (index >= 100) {
        indexMessage.append(static_cast<char>(48 + trim / 100));
        trim = trim % 100;
    }
    if (index >= 10) {
        indexMessage.append(static_cast<char>(48 + trim / 10)); 
        trim = trim % 10;
    }
    if (index >= 0) {
        indexMessage.append(static_cast<char>(48 + trim));
    }
    //setMessage(1, indexMessage.cstr());
    indexMessage = "";
    indexMessage.append("Current item index ");
    index = getCheckIndex();
    trim = index;
    if (index >= 1000)
    {
        indexMessage.append(static_cast<char>(48 + trim / 1000));
        trim = trim % 1000;
    }
    if (index >= 100) {
        indexMessage.append(static_cast<char>(48 + trim / 100));
        trim = trim % 100;
    }
    if (index >= 10) {
        indexMessage.append(static_cast<char>(48 + trim / 10)); 
        trim = trim % 10;
    }
    if (index >= 0) {
        indexMessage.append(static_cast<char>(48 + trim));
    }
    //setMessage(2, indexMessage.cstr());

    switch (itemType)
    { 
    case -2:
        //setMessage(3, "Coins Received");
        if (getCheckIndex() < packet->index)
        {
            GameDataFunction::addCoin(accessor, packet->amount);
            updateIndex = true;
        }
        break;
    case -1:
        if (collectedShineCount < curCollectedShines.size() - 1) {
            curCollectedShines[collectedShineCount] = packet->locationId;
            collectedShineCount++;
        }
        break;
    case 0:
        strcpy(info.mName, costumeNames[packet->locationId]);
        info.mType = static_cast<ShopItem::ItemType>(itemType);
        infoPtr = &info;
        accessor.mData->mGameDataFile->buyItem(infoPtr, false);
        if (getCheckIndex() < packet->index) {
            GameDataFunction::wearCostume(accessor, info.mName);
            updateIndex = true;
        }
        break;
    case 1:
        strcpy(info.mName, costumeNames[packet->locationId]);
        info.mType = static_cast<ShopItem::ItemType>(itemType);
        infoPtr = &info;
        accessor.mData->mGameDataFile->buyItem(infoPtr, false);
        if (getCheckIndex() < packet->index) {
            GameDataFunction::wearCap(accessor, info.mName);
            updateIndex = true;
        }
        break;
    case 2:
        strcpy(info.mName, souvenirNames[packet->locationId]);
        info.mType = static_cast<ShopItem::ItemType>(itemType);
        infoPtr = &info;
        accessor.mData->mGameDataFile->buyItem(infoPtr, false);
        break;
    case 3:
        strcpy(info.mName, stickerNames[packet->locationId]);
        info.mType = static_cast<ShopItem::ItemType>(itemType);
        infoPtr = &info;
        accessor.mData->mGameDataFile->buyItem(infoPtr, false);
        break;

    case 5:
        addCapture(captureListNames[packet->locationId]);
        GameDataFunction::addHackDictionary(accessor, captureListNames[packet->locationId]);
        break;
    }

    if (updateIndex) {
        setCheckIndex(packet->index);
    }

}

void Client::updateSentShines(ShineChecks* packet)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    addShine(packet->shineUid0);
    addShine(packet->shineUid1);
    addShine(packet->shineUid2);
    addShine(packet->shineUid3);
    addShine(packet->shineUid4);
    addShine(packet->shineUid5);
    addShine(packet->shineUid6);
    addShine(packet->shineUid7);
    addShine(packet->shineUid8);
    addShine(packet->shineUid9);
    addShine(packet->shineUid10);
    addShine(packet->shineUid11);
    addShine(packet->shineUid12);
    addShine(packet->shineUid13);
    addShine(packet->shineUid14);
    addShine(packet->shineUid15);
    addShine(packet->shineUid16);
    addShine(packet->shineUid17);
    addShine(packet->shineUid18);
    addShine(packet->shineUid19);
    addShine(packet->shineUid20);
    addShine(packet->shineUid21);
    addShine(packet->shineUid22);
    addShine(packet->shineUid23);
    addShine(packet->shineUid24);
    addShine(packet->shineUid25);
    addShine(packet->shineUid26);
    addShine(packet->shineUid27);
    addShine(packet->shineUid28);
    addShine(packet->shineUid29);
    addShine(packet->shineUid30);
    addShine(packet->shineUid31);
    addShine(packet->shineUid32);
    addShine(packet->shineUid33);
    addShine(packet->shineUid34);
    addShine(packet->shineUid35);
    addShine(packet->shineUid36);
    addShine(packet->shineUid37);
    addShine(packet->shineUid38);
    addShine(packet->shineUid39);
    addShine(packet->shineUid40);
    addShine(packet->shineUid41);
    addShine(packet->shineUid42);
    addShine(packet->shineUid43);
    addShine(packet->shineUid44);
    addShine(packet->shineUid45);
    addShine(packet->shineUid46);
    addShine(packet->shineUid47);
    addShine(packet->shineUid48);
    addShine(packet->shineUid49);
    addShine(packet->shineUid50);
    addShine(packet->shineUid51);
    addShine(packet->shineUid52);
    addShine(packet->shineUid53);
    addShine(packet->shineUid54);
    addShine(packet->shineUid55);
    addShine(packet->shineUid56);
    addShine(packet->shineUid57);
    addShine(packet->shineUid58);
    addShine(packet->shineUid59);
    addShine(packet->shineUid60);
    addShine(packet->shineUid61);
    addShine(packet->shineUid62);
    addShine(packet->shineUid63);
    addShine(packet->shineUid64);
    addShine(packet->shineUid65);
    addShine(packet->shineUid66);
    addShine(packet->shineUid67);
    addShine(packet->shineUid68);
    addShine(packet->shineUid69);
    addShine(packet->shineUid70);
    addShine(packet->shineUid71);
    addShine(packet->shineUid72);
    addShine(packet->shineUid73);
    addShine(packet->shineUid74);
    addShine(packet->shineUid75);
    addShine(packet->shineUid76);
    addShine(packet->shineUid77);
    addShine(packet->shineUid78);
    addShine(packet->shineUid79);
    addShine(packet->shineUid80);
    addShine(packet->shineUid81);
    addShine(packet->shineUid82);
    addShine(packet->shineUid83);
    addShine(packet->shineUid84);
    addShine(packet->shineUid85);
    addShine(packet->shineUid86);
    addShine(packet->shineUid87);
    addShine(packet->shineUid88);
    addShine(packet->shineUid89);
    addShine(packet->shineUid90);
    addShine(packet->shineUid91);
    addShine(packet->shineUid92);
    addShine(packet->shineUid93);
    addShine(packet->shineUid94);
    addShine(packet->shineUid95);
    addShine(packet->shineUid96);
    addShine(packet->shineUid97);
    addShine(packet->shineUid98);
    addShine(packet->shineUid99);
}

int Client::getWorldUnlockCount(int worldId) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return 0;
    }

    return sInstance->worldPayCounts[worldId];
}

void Client::addShine(int uid)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    int shines = sInstance->collectedShines[uid / 32];

    int index = (uid / 32) * 32;
    int i = 1;
    while (i > 0) {
        if (index == uid) {
            shines = shines | i;
            break;
        }
        i = i << 1;
        index += 1;
    }


    sInstance->collectedShines[uid / 32] = shines;
}

void Client::setRecentShine(Shine* curShine)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }
    sInstance->recentShine = curShine;
}

bool Client::hasShine(int uid)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return false;
    }

    int shines = sInstance->collectedShines[uid / 32];

    int index = (uid / 32) * 32;
    int i = 1;
    while (i > 0) {
        if (index == uid) {
            shines = shines & i;
            return (shines == i);
        }
        i = i << 1;
        index += 1;
    }
    return false;
}

int Client::getShineChecks(int index)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return 0;
    }

    return sInstance->collectedShines[index];
}

void Client::setShineChecks(int index, int checks)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sInstance->collectedShines[index] = checks;
}

void Client::addOutfit(const ShopItem::ItemInfo *info)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    int index = getIndexCostumeList(info->mName) + 44 * static_cast<int>(info->mType);

    int outfits = sInstance->collectedOutfits[index / 8];

    int curIndex = (index / 8) * 8;
    int i = 1;
    while (i < 0x100) {
        if (curIndex == index) {
            outfits = outfits | i;
            break;
        }
        i = i << 1;
        curIndex += 1;
    }


    sInstance->collectedOutfits[index / 8] = outfits;
}

bool Client::hasOutfit(const ShopItem::ItemInfo *info)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return false;
    }

    int index = getIndexCostumeList(info->mName) + 44 * static_cast<int>(info->mType);
    if (index == -1) {
        //setMessage(2, info->mName);
        return false;
    }

    u8 outfits = sInstance->collectedOutfits[index / 8];

    int curIndex = (index / 8) * 8;
    int i = 1;
    while (i < 0x100) {
        if (curIndex == index) {
            outfits = outfits & i;
            return (outfits == i);
        }
        i = i << 1;
        curIndex += 1;
    }
}

int Client::getOutfitChecks(int index)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return 0;
    }

    return static_cast<int>(sInstance->collectedOutfits[index]);
}

void Client::setOutfitChecks(int index, int checks) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }
    u8 u8Checks = static_cast<u8>(checks);
    sInstance->collectedOutfits[index] = u8Checks;
}

void Client::addSticker(const ShopItem::ItemInfo *info)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    int index = getIndexStickerList(info->mName);

    int stickers = sInstance->collectedStickers[index / 8];

    int curIndex = (index / 8) * 8;
    int i = 1;
    while (i < 0x100) {
        if (curIndex == index) {
            stickers = stickers | i;
            break;
        }
        i = i << 1;
        curIndex += 1;
    }


    sInstance->collectedStickers[index / 8] = stickers;
}

bool Client::hasSticker(const ShopItem::ItemInfo *info)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return false;
    }

    int index = getIndexStickerList(info->mName);
    if (index == -1) {
        //setMessage(2, info->mName);
        return false;
    }

    u8 stickers = sInstance->collectedStickers[index / 8];

    int curIndex = (index / 8) * 8;
    int i = 1;
    while (i < 0x100) {
        if (curIndex == index) {
            stickers = stickers & i;
            return (stickers == i);
        }
        i = i << 1;
        curIndex += 1;
    }
}

int Client::getStickerChecks(int index) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return 0;
    }

    return static_cast<int>(sInstance->collectedStickers[index]);
}

void Client::setStickerChecks(int index, int checks) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }
    u8 u8Checks = static_cast<u8>(checks);
    sInstance->collectedStickers[index] = u8Checks;
}

void Client::addSouvenir(const ShopItem::ItemInfo *info)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    int index = getIndexSouvenirList(info->mName);

    int souvenirs = sInstance->collectedSouvenirs[index / 8];

    int curIndex = (index / 8) * 8;
    int i = 1;
    while (i < 0x100) {
        if (curIndex == index) {
            souvenirs = souvenirs | i;
            break;
        }
        i = i << 1;
        curIndex += 1;
    }


    sInstance->collectedSouvenirs[index / 8] = souvenirs;
}

bool Client::hasSouvenir(const ShopItem::ItemInfo *info)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return false;
    }

    int index = getIndexSouvenirList(info->mName);
    if (index == -1) {
        //setMessage(2, info->mName);
        return false;
    }

    u8 souvenirs = sInstance->collectedSouvenirs[index / 8];

    int curIndex = (index / 8) * 8;
    int i = 1;
    while (i < 0x100) {
        if (curIndex == index) {
            souvenirs = souvenirs & i;
            return (souvenirs == i);
        }
        i = i << 1;
        curIndex += 1;
    }
}

int Client::getSouvenirChecks(int index) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return 0;
    }

    return static_cast<int>(sInstance->collectedSouvenirs[index]);
}

void Client::setSouvenirChecks(int index, int checks) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }
    u8 u8Checks = static_cast<u8>(checks);
    sInstance->collectedSouvenirs[index] = u8Checks;
}

bool Client::hasItem(const ShopItem::ItemInfo* info)
{
    switch (static_cast<int>(info->mType))
    { 
        case 1:
            return hasOutfit(info);
        case 0:
            return hasOutfit(info);
        case 3:
            return hasSticker(info);
        case 2:
            return hasSouvenir(info);
        default:
            // Moon and useitem
            return false;
    }
}

void Client::addItem(const ShopItem::ItemInfo* info)
{
    switch (static_cast<int>(info->mType)) {
        case 1:
            addOutfit(info);
            break;
        case 0:
            addOutfit(info);
            break;
        case 3:
            addSticker(info);
            break;
        case 2:
            addSouvenir(info);
            break;
        default:
            // Moon and useitem
            break;
    }
}

void Client::addCapture(const char *capture)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    int index = getIndexCaptureList(capture);

    int checkedCaptures = sInstance->collectedCaptures[index / 8];

    int curIndex = (index / 8) * 8;
    int i = 1;
    while (i < 0x100) {
        if (curIndex == index) {
            checkedCaptures = checkedCaptures | i;
            break;
        }
        i = i << 1;
        curIndex += 1;
    }


    sInstance->collectedCaptures[index / 8] = checkedCaptures;
}

bool Client::hasCapture(const char* capture) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return false;
    }

    int index = getIndexCaptureList(capture);
    if (index == -1)
    {
        sead::FixedSafeString<40> str;
        str = "\x1b[33m";
        str.append(capture);
        str.append(" not in captures list.\x1b[0m");
        appendMessage(str.cstr());
        return false;
    }

    u8 checkedCaptures = sInstance->collectedCaptures[index / 8];

    int curIndex = (index / 8) * 8;
    int i = 1;
    while (i < 0x100) {
        if (curIndex == index) {
            checkedCaptures = checkedCaptures & i;
            return (checkedCaptures == i);
        }
        i = i << 1;
        curIndex += 1;
    }
    return false;
}

int Client::getCaptureChecks(int index) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return 0;
    }

    return static_cast<int>(sInstance->collectedCaptures[index]);
}

void Client::setCaptureChecks(int index, int checks) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    u8 u8Checks = static_cast<u8>(checks);
    sInstance->collectedCaptures[index] = u8Checks;
}

void Client::addCaptureCheck(const char* capture) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    int index = getIndexCaptureList(capture);

    int checkedCaptures = sInstance->checkedCaptures[index / 8];

    int curIndex = (index / 8) * 8;
    int i = 1;
    while (i < 0x100) {
        if (curIndex == index) {
            checkedCaptures = checkedCaptures | i;
            break;
        }
        i = i << 1;
        curIndex += 1;
    }

    sInstance->checkedCaptures[index / 8] = checkedCaptures;
}

bool Client::hasCaptureCheck(const char* capture) {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return false;
    }

    int index = getIndexCaptureList(capture);
    if (index == -1) {
        sead::FixedSafeString<40> str;
        str = "\x1b[33m";
        str.append(capture);
        str.append(" not in captures list.\x1b[0m");
        appendMessage(str.cstr());
        return false;
    }

    u8 checkedCaptures = sInstance->checkedCaptures[index / 8];

    int curIndex = (index / 8) * 8;
    int i = 1;
    while (i < 0x100) {
        if (curIndex == index) {
            checkedCaptures = checkedCaptures & i;
            return (checkedCaptures == i);
        }
        i = i << 1;
        curIndex += 1;
    }
    return false;
}

void Client::startShineCount() {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }
    sInstance->mCurStageScene->mSceneLayout->startShineCountAnim(false);
    startShineChipCount();
}

void Client::startShineChipCount() {
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }
    sInstance->mCurStageScene->mSceneLayout->updateCounterParts();  // updates shine chip layout to (maybe) prevent softlocks
}

void Client::appendMessage(const char* msg)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    if (strlen(msg) <= 0)
    {
        return;
    }

    if (sInstance->apChatLineCount >= 5)
    {
        shiftMessages();
    }

    sInstance->apChatLines[sInstance->apChatLineCount] = msg;
    sInstance->apChatLineCount++;
}

void Client::shiftMessages()
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    for (int i = 1; i < sInstance->apChatLineCount; i++)
        sInstance->apChatLines[i - 1] = sInstance->apChatLines[i];

    sInstance->apChatLineCount--;
}

void Client::addApInfo(ApInfo* packet)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    int type = static_cast<int>(packet->infoType);

    if (type < 3) 
    {
        sead::WFixedSafeString<40> info1;
        sead::WFixedSafeString<40> info2;
        sead::WFixedSafeString<40> info3;
        info1 = info1.cEmptyString;
        info2 = info2.cEmptyString;
        info3 = info3.cEmptyString;

        for (int i = 0; i < 40; i++) {
            if (packet->info1[i] == '\0') {
                break;
            }
            info1.append(static_cast<char16>(packet->info1[i]));
        }

        for (int i = 0; i < 40; i++) {
            if (packet->info2[i] == '\0') {
                break;
            }
            info2.append(static_cast<char16>(packet->info2[i]));
        }

        for (int i = 0; i < 40; i++) {
            if (packet->info3[i] == '\0') {
                break;
            }
            info3.append(static_cast<char16>(packet->info3[i]));
        }

        // setMessage(2, "AP Info Entered");

        if (type == 0) {
            // setMessage(1, "Game Info Entered");
            // if (!info1.isEmpty())
            //{
            /*if (packet->index1 == 0)
            {
                setMessage(1, packet->info1);
            }*/
            sInstance->apGameNames[packet->index1] =
                sInstance->apGameNames[packet->index1].cEmptyString;
            sInstance->apGameNames[packet->index1].append(info1.cstr());
            sInstance->numApGames++;
            //}
            // if (!info2.isEmpty())
            //{
            sInstance->apGameNames[packet->index2] =
                sInstance->apGameNames[packet->index2].cEmptyString;
            sInstance->apGameNames[packet->index2].append(info2.cstr());
            sInstance->numApGames++;
            //}
            // if (!info3.isEmpty())
            //{
            sInstance->apGameNames[packet->index3] =
                sInstance->apGameNames[packet->index3].cEmptyString;
            sInstance->apGameNames[packet->index3].append(info3.cstr());
            sInstance->numApGames++;
            //}
        }

        if (type == 1) {
            // if (!info1.isEmpty())
            //{
            sInstance->apSlotNames[packet->index1] =
                sInstance->apSlotNames[packet->index1].cEmptyString;
            sInstance->apSlotNames[packet->index1].append(info1.cstr());
            sInstance->numApSlots++;
            //}
            // if (!info2.isEmpty())
            //{
            sInstance->apSlotNames[packet->index2] =
                sInstance->apSlotNames[packet->index2].cEmptyString;
            sInstance->apSlotNames[packet->index2].append(info2.cstr());
            sInstance->numApSlots++;
            //}
            // if (!info3.isEmpty())
            //{
            sInstance->apSlotNames[packet->index3] =
                sInstance->apSlotNames[packet->index3].cEmptyString;
            sInstance->apSlotNames[packet->index3].append(info3.cstr());
            sInstance->numApSlots++;
            //}
        }

        if (type == 2) {
            // if (!info1.isEmpty())
            //{
            sInstance->apItemNames[packet->index1] =
                sInstance->apItemNames[packet->index1].cEmptyString;
            sInstance->apItemNames[packet->index1].append(info1.cstr());
            sInstance->numApItems++;
            //}
            // if (!info2.isEmpty())
            //{
            sInstance->apItemNames[packet->index2] =
                sInstance->apItemNames[packet->index2].cEmptyString;
            sInstance->apItemNames[packet->index2].append(info2.cstr());
            sInstance->numApItems++;
            //}
            // if (!info3.isEmpty())
            //{
            sInstance->apItemNames[packet->index3] =
                sInstance->apItemNames[packet->index3].cEmptyString;
            sInstance->apItemNames[packet->index3].append(info3.cstr());
            sInstance->numApItems++;
            //}
        }
    } 
    else 
    {
        if (type == 3) {
            sInstance->shineItemNames[packet->index1] =
                sInstance->shineItemNames[packet->index1].cEmptyString;
            sInstance->shineItemNames[packet->index1].append(packet->info1);

            if (packet->index1 < 99) {
                sInstance->shineItemNames[packet->index2] =
                    sInstance->shineItemNames[packet->index2].cEmptyString;
                sInstance->shineItemNames[packet->index2].append(packet->info2);

                sInstance->shineItemNames[packet->index3] =
                    sInstance->shineItemNames[packet->index3].cEmptyString;
                sInstance->shineItemNames[packet->index3].append(packet->info3);
            }

        }
    }

}

void Client::updateShineReplace(ShineReplacePacket* packet)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }

    sInstance->shineTextReplacements[0] = {packet->itemType0, packet->itemNameIndex0};
    sInstance->shineTextReplacements[1] = {packet->itemType1, packet->itemNameIndex1};
    sInstance->shineTextReplacements[2] = {packet->itemType2, packet->itemNameIndex2};
    sInstance->shineTextReplacements[3] = {packet->itemType3, packet->itemNameIndex3};
    sInstance->shineTextReplacements[4] = {packet->itemType4, packet->itemNameIndex4};
    sInstance->shineTextReplacements[5] = {packet->itemType5, packet->itemNameIndex5};
    sInstance->shineTextReplacements[6] = {packet->itemType6, packet->itemNameIndex6};
    sInstance->shineTextReplacements[7] = {packet->itemType7, packet->itemNameIndex7};
    sInstance->shineTextReplacements[8] = {packet->itemType8, packet->itemNameIndex8};
    sInstance->shineTextReplacements[9] = {packet->itemType9, packet->itemNameIndex9};
    sInstance->shineTextReplacements[10] = {packet->itemType10, packet->itemNameIndex10};
    sInstance->shineTextReplacements[11] = {packet->itemType11, packet->itemNameIndex11};
    sInstance->shineTextReplacements[12] = {packet->itemType12, packet->itemNameIndex12};
    sInstance->shineTextReplacements[13] = {packet->itemType13, packet->itemNameIndex13};
    sInstance->shineTextReplacements[14] = {packet->itemType14, packet->itemNameIndex14};
    sInstance->shineTextReplacements[15] = {packet->itemType15, packet->itemNameIndex15};
    sInstance->shineTextReplacements[16] = {packet->itemType16, packet->itemNameIndex16};
    sInstance->shineTextReplacements[17] = {packet->itemType17, packet->itemNameIndex17};
    sInstance->shineTextReplacements[18] = {packet->itemType18, packet->itemNameIndex18};
    sInstance->shineTextReplacements[19] = {packet->itemType19, packet->itemNameIndex19};
    sInstance->shineTextReplacements[20] = {packet->itemType20, packet->itemNameIndex20};
    sInstance->shineTextReplacements[21] = {packet->itemType21, packet->itemNameIndex21};
    sInstance->shineTextReplacements[22] = {packet->itemType22, packet->itemNameIndex22};
    sInstance->shineTextReplacements[23] = {packet->itemType23, packet->itemNameIndex23};
    sInstance->shineTextReplacements[24] = {packet->itemType24, packet->itemNameIndex24};
    sInstance->shineTextReplacements[25] = {packet->itemType25, packet->itemNameIndex25};
    sInstance->shineTextReplacements[26] = {packet->itemType26, packet->itemNameIndex26};
    sInstance->shineTextReplacements[27] = {packet->itemType27, packet->itemNameIndex27};
    sInstance->shineTextReplacements[28] = {packet->itemType28, packet->itemNameIndex28};
    sInstance->shineTextReplacements[29] = {packet->itemType29, packet->itemNameIndex29};
    sInstance->shineTextReplacements[30] = {packet->itemType30, packet->itemNameIndex30};
    sInstance->shineTextReplacements[31] = {packet->itemType31, packet->itemNameIndex31};
    sInstance->shineTextReplacements[32] = {packet->itemType32, packet->itemNameIndex32};
    sInstance->shineTextReplacements[33] = {packet->itemType33, packet->itemNameIndex33};
    sInstance->shineTextReplacements[34] = {packet->itemType34, packet->itemNameIndex34};
    sInstance->shineTextReplacements[35] = {packet->itemType35, packet->itemNameIndex35};
    sInstance->shineTextReplacements[36] = {packet->itemType36, packet->itemNameIndex36};
    sInstance->shineTextReplacements[37] = {packet->itemType37, packet->itemNameIndex37};
    sInstance->shineTextReplacements[38] = {packet->itemType38, packet->itemNameIndex38};
    sInstance->shineTextReplacements[39] = {packet->itemType39, packet->itemNameIndex39};
    sInstance->shineTextReplacements[40] = {packet->itemType40, packet->itemNameIndex40};
    sInstance->shineTextReplacements[41] = {packet->itemType41, packet->itemNameIndex41};
    sInstance->shineTextReplacements[42] = {packet->itemType42, packet->itemNameIndex42};
    sInstance->shineTextReplacements[43] = {packet->itemType43, packet->itemNameIndex43};
    sInstance->shineTextReplacements[44] = {packet->itemType44, packet->itemNameIndex44};
    sInstance->shineTextReplacements[45] = {packet->itemType45, packet->itemNameIndex45};
    sInstance->shineTextReplacements[46] = {packet->itemType46, packet->itemNameIndex46};
    sInstance->shineTextReplacements[47] = {packet->itemType47, packet->itemNameIndex47};
    sInstance->shineTextReplacements[48] = {packet->itemType48, packet->itemNameIndex48};
    sInstance->shineTextReplacements[49] = {packet->itemType49, packet->itemNameIndex49};
    sInstance->shineTextReplacements[50] = {packet->itemType50, packet->itemNameIndex50};
    sInstance->shineTextReplacements[51] = {packet->itemType51, packet->itemNameIndex51};
    sInstance->shineTextReplacements[52] = {packet->itemType52, packet->itemNameIndex52};
    sInstance->shineTextReplacements[53] = {packet->itemType53, packet->itemNameIndex53};
    sInstance->shineTextReplacements[54] = {packet->itemType54, packet->itemNameIndex54};
    sInstance->shineTextReplacements[55] = {packet->itemType55, packet->itemNameIndex55};
    sInstance->shineTextReplacements[56] = {packet->itemType56, packet->itemNameIndex56};
    sInstance->shineTextReplacements[57] = {packet->itemType57, packet->itemNameIndex57};
    sInstance->shineTextReplacements[58] = {packet->itemType58, packet->itemNameIndex58};
    sInstance->shineTextReplacements[59] = {packet->itemType59, packet->itemNameIndex59};
    sInstance->shineTextReplacements[60] = {packet->itemType60, packet->itemNameIndex60};
    sInstance->shineTextReplacements[61] = {packet->itemType61, packet->itemNameIndex61};
    sInstance->shineTextReplacements[62] = {packet->itemType62, packet->itemNameIndex62};
    sInstance->shineTextReplacements[63] = {packet->itemType63, packet->itemNameIndex63};
    sInstance->shineTextReplacements[64] = {packet->itemType64, packet->itemNameIndex64};
    sInstance->shineTextReplacements[65] = {packet->itemType65, packet->itemNameIndex65};
    sInstance->shineTextReplacements[66] = {packet->itemType66, packet->itemNameIndex66};
    sInstance->shineTextReplacements[67] = {packet->itemType67, packet->itemNameIndex67};
    sInstance->shineTextReplacements[68] = {packet->itemType68, packet->itemNameIndex68};
    sInstance->shineTextReplacements[69] = {packet->itemType69, packet->itemNameIndex69};
    sInstance->shineTextReplacements[70] = {packet->itemType70, packet->itemNameIndex70};
    sInstance->shineTextReplacements[71] = {packet->itemType71, packet->itemNameIndex71};
    sInstance->shineTextReplacements[72] = {packet->itemType72, packet->itemNameIndex72};
    sInstance->shineTextReplacements[73] = {packet->itemType73, packet->itemNameIndex73};
    sInstance->shineTextReplacements[74] = {packet->itemType74, packet->itemNameIndex74};
    sInstance->shineTextReplacements[75] = {packet->itemType75, packet->itemNameIndex75};
    sInstance->shineTextReplacements[76] = {packet->itemType76, packet->itemNameIndex76};
    sInstance->shineTextReplacements[77] = {packet->itemType77, packet->itemNameIndex77};
    sInstance->shineTextReplacements[78] = {packet->itemType78, packet->itemNameIndex78};
    sInstance->shineTextReplacements[79] = {packet->itemType79, packet->itemNameIndex79};
    sInstance->shineTextReplacements[80] = {packet->itemType80, packet->itemNameIndex80};
    sInstance->shineTextReplacements[81] = {packet->itemType81, packet->itemNameIndex81};
    sInstance->shineTextReplacements[82] = {packet->itemType82, packet->itemNameIndex82};
    sInstance->shineTextReplacements[83] = {packet->itemType83, packet->itemNameIndex83};
    sInstance->shineTextReplacements[84] = {packet->itemType84, packet->itemNameIndex84};
    sInstance->shineTextReplacements[85] = {packet->itemType85, packet->itemNameIndex85};
    sInstance->shineTextReplacements[86] = {packet->itemType86, packet->itemNameIndex86};
    sInstance->shineTextReplacements[87] = {packet->itemType87, packet->itemNameIndex87};
    sInstance->shineTextReplacements[88] = {packet->itemType88, packet->itemNameIndex88};
    sInstance->shineTextReplacements[89] = {packet->itemType89, packet->itemNameIndex89};
    sInstance->shineTextReplacements[90] = {packet->itemType90, packet->itemNameIndex90};
    sInstance->shineTextReplacements[91] = {packet->itemType91, packet->itemNameIndex91};
    sInstance->shineTextReplacements[92] = {packet->itemType92, packet->itemNameIndex92};
    sInstance->shineTextReplacements[93] = {packet->itemType93, packet->itemNameIndex93};
    sInstance->shineTextReplacements[94] = {packet->itemType94, packet->itemNameIndex94};
    sInstance->shineTextReplacements[95] = {packet->itemType95, packet->itemNameIndex95};
    sInstance->shineTextReplacements[96] = {packet->itemType96, packet->itemNameIndex96};
    sInstance->shineTextReplacements[97] = {packet->itemType97, packet->itemNameIndex97};
    sInstance->shineTextReplacements[98] = {packet->itemType98, packet->itemNameIndex98};
    sInstance->shineTextReplacements[99] = {packet->itemType99, packet->itemNameIndex99};
}

void Client::updateShineColor(ShineColor* packet)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }
    //setMessage(1, "Entering udpateShineColor");
    sInstance->shineColors[static_cast<int>(packet->shineUid0)] = packet->color0;
    sInstance->shineColors[static_cast<int>(packet->shineUid1)] = packet->color1;
    sInstance->shineColors[static_cast<int>(packet->shineUid2)] = packet->color2;
    sInstance->shineColors[static_cast<int>(packet->shineUid3)] = packet->color3;
    sInstance->shineColors[static_cast<int>(packet->shineUid4)] = packet->color4;
    sInstance->shineColors[static_cast<int>(packet->shineUid5)] = packet->color5;
    sInstance->shineColors[static_cast<int>(packet->shineUid6)] = packet->color6;
    sInstance->shineColors[static_cast<int>(packet->shineUid7)] = packet->color7;
    sInstance->shineColors[static_cast<int>(packet->shineUid8)] = packet->color8;
    sInstance->shineColors[static_cast<int>(packet->shineUid9)] = packet->color9;
    sInstance->shineColors[static_cast<int>(packet->shineUid10)] = packet->color10;
    sInstance->shineColors[static_cast<int>(packet->shineUid11)] = packet->color11;
    sInstance->shineColors[static_cast<int>(packet->shineUid12)] = packet->color12;
    sInstance->shineColors[static_cast<int>(packet->shineUid13)] = packet->color13;
    sInstance->shineColors[static_cast<int>(packet->shineUid14)] = packet->color14;
    sInstance->shineColors[static_cast<int>(packet->shineUid15)] = packet->color15;
    sInstance->shineColors[static_cast<int>(packet->shineUid16)] = packet->color16;
    sInstance->shineColors[static_cast<int>(packet->shineUid17)] = packet->color17;
    sInstance->shineColors[static_cast<int>(packet->shineUid18)] = packet->color18;
    sInstance->shineColors[static_cast<int>(packet->shineUid19)] = packet->color19;
    sInstance->shineColors[static_cast<int>(packet->shineUid20)] = packet->color20;
    sInstance->shineColors[static_cast<int>(packet->shineUid21)] = packet->color21;
    sInstance->shineColors[static_cast<int>(packet->shineUid22)] = packet->color22;
    sInstance->shineColors[static_cast<int>(packet->shineUid23)] = packet->color23;
    sInstance->shineColors[static_cast<int>(packet->shineUid24)] = packet->color24;
    sInstance->shineColors[static_cast<int>(packet->shineUid25)] = packet->color25;
    sInstance->shineColors[static_cast<int>(packet->shineUid26)] = packet->color26;
    sInstance->shineColors[static_cast<int>(packet->shineUid27)] = packet->color27;
    sInstance->shineColors[static_cast<int>(packet->shineUid28)] = packet->color28;
    sInstance->shineColors[static_cast<int>(packet->shineUid29)] = packet->color29;
    sInstance->shineColors[static_cast<int>(packet->shineUid30)] = packet->color30;
    sInstance->shineColors[static_cast<int>(packet->shineUid31)] = packet->color31;
    sInstance->shineColors[static_cast<int>(packet->shineUid32)] = packet->color32;
    sInstance->shineColors[static_cast<int>(packet->shineUid33)] = packet->color33;
    sInstance->shineColors[static_cast<int>(packet->shineUid34)] = packet->color34;
    sInstance->shineColors[static_cast<int>(packet->shineUid35)] = packet->color35;
    sInstance->shineColors[static_cast<int>(packet->shineUid36)] = packet->color36;
    sInstance->shineColors[static_cast<int>(packet->shineUid37)] = packet->color37;
    sInstance->shineColors[static_cast<int>(packet->shineUid38)] = packet->color38;
    sInstance->shineColors[static_cast<int>(packet->shineUid39)] = packet->color39;
    sInstance->shineColors[static_cast<int>(packet->shineUid40)] = packet->color40;
    sInstance->shineColors[static_cast<int>(packet->shineUid41)] = packet->color41;
    sInstance->shineColors[static_cast<int>(packet->shineUid42)] = packet->color42;
    sInstance->shineColors[static_cast<int>(packet->shineUid43)] = packet->color43;
    sInstance->shineColors[static_cast<int>(packet->shineUid44)] = packet->color44;
    sInstance->shineColors[static_cast<int>(packet->shineUid45)] = packet->color45;
    sInstance->shineColors[static_cast<int>(packet->shineUid46)] = packet->color46;
    sInstance->shineColors[static_cast<int>(packet->shineUid47)] = packet->color47;
    sInstance->shineColors[static_cast<int>(packet->shineUid48)] = packet->color48;
    sInstance->shineColors[static_cast<int>(packet->shineUid49)] = packet->color49;
    sInstance->shineColors[static_cast<int>(packet->shineUid50)] = packet->color50;
    //sInstance->shineColors[static_cast<int>(packet->shineUid51)] = packet->color51;
    //sInstance->shineColors[static_cast<int>(packet->shineUid52)] = packet->color52;
    //sInstance->shineColors[static_cast<int>(packet->shineUid53)] = packet->color53;
    //sInstance->shineColors[static_cast<int>(packet->shineUid54)] = packet->color54;
    //sInstance->shineColors[static_cast<int>(packet->shineUid55)] = packet->color55;
    //sInstance->shineColors[static_cast<int>(packet->shineUid56)] = packet->color56;
    //sInstance->shineColors[static_cast<int>(packet->shineUid57)] = packet->color57;
    //sInstance->shineColors[static_cast<int>(packet->shineUid58)] = packet->color58;
    //sInstance->shineColors[static_cast<int>(packet->shineUid59)] = packet->color59;
    //sInstance->shineColors[static_cast<int>(packet->shineUid60)] = packet->color60;
    //sInstance->shineColors[static_cast<int>(packet->shineUid61)] = packet->color61;
    //sInstance->shineColors[static_cast<int>(packet->shineUid62)] = packet->color62;
    //sInstance->shineColors[static_cast<int>(packet->shineUid63)] = packet->color63;
    //sInstance->shineColors[static_cast<int>(packet->shineUid64)] = packet->color64;
    //sInstance->shineColors[static_cast<int>(packet->shineUid65)] = packet->color65;
    //sInstance->shineColors[static_cast<int>(packet->shineUid66)] = packet->color66;
    //sInstance->shineColors[static_cast<int>(packet->shineUid67)] = packet->color67;
    //sInstance->shineColors[static_cast<int>(packet->shineUid68)] = packet->color68;
    //sInstance->shineColors[static_cast<int>(packet->shineUid69)] = packet->color69;
    //sInstance->shineColors[static_cast<int>(packet->shineUid70)] = packet->color70;
    //sInstance->shineColors[static_cast<int>(packet->shineUid71)] = packet->color71;
    //sInstance->shineColors[static_cast<int>(packet->shineUid72)] = packet->color72;
    //sInstance->shineColors[static_cast<int>(packet->shineUid73)] = packet->color73;
    //sInstance->shineColors[static_cast<int>(packet->shineUid74)] = packet->color74;
    //sInstance->shineColors[static_cast<int>(packet->shineUid75)] = packet->color75;
    //sInstance->shineColors[static_cast<int>(packet->shineUid76)] = packet->color76;
    //sInstance->shineColors[static_cast<int>(packet->shineUid77)] = packet->color77;
    //sInstance->shineColors[static_cast<int>(packet->shineUid78)] = packet->color78;
    //sInstance->shineColors[static_cast<int>(packet->shineUid79)] = packet->color79;
    //sInstance->shineColors[static_cast<int>(packet->shineUid80)] = packet->color80;
    //sInstance->shineColors[static_cast<int>(packet->shineUid81)] = packet->color81;
    //sInstance->shineColors[static_cast<int>(packet->shineUid82)] = packet->color82;
    //sInstance->shineColors[static_cast<int>(packet->shineUid83)] = packet->color83;
}

void Client::updateShopReplace(ShopReplacePacket* packet)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return;
    }
    int type = static_cast<int>(packet->infoType);
    // Cap
    if (type == 0)
    {
        sInstance->shopCapTextReplacements[0] = {packet->gameIndex0, packet->playerIndex0,
                                                 packet->itemIndex0, packet->itemClassification0};
        sInstance->shopCapTextReplacements[1] = {packet->gameIndex1, packet->playerIndex1,
                                                 packet->itemIndex1, packet->itemClassification1};
        sInstance->shopCapTextReplacements[2] = {packet->gameIndex2, packet->playerIndex2,
                                                 packet->itemIndex2, packet->itemClassification2};
        sInstance->shopCapTextReplacements[3] = {packet->gameIndex3, packet->playerIndex3,
                                                 packet->itemIndex3, packet->itemClassification3};
        sInstance->shopCapTextReplacements[4] = {packet->gameIndex4, packet->playerIndex4,
                                                 packet->itemIndex4, packet->itemClassification4};
        sInstance->shopCapTextReplacements[5] = {packet->gameIndex5, packet->playerIndex5,
                                                 packet->itemIndex5, packet->itemClassification5};
        sInstance->shopCapTextReplacements[6] = {packet->gameIndex6, packet->playerIndex6,
                                                 packet->itemIndex6, packet->itemClassification6};
        sInstance->shopCapTextReplacements[7] = {packet->gameIndex7, packet->playerIndex7,
                                                 packet->itemIndex7, packet->itemClassification7};
        sInstance->shopCapTextReplacements[8] = {packet->gameIndex8, packet->playerIndex8,
                                                 packet->itemIndex8, packet->itemClassification8};
        sInstance->shopCapTextReplacements[9] = {packet->gameIndex9, packet->playerIndex9,
                                                 packet->itemIndex9, packet->itemClassification9};
        sInstance->shopCapTextReplacements[10] = {packet->gameIndex10, packet->playerIndex10,
                                                  packet->itemIndex10,
                                                  packet->itemClassification10};
        sInstance->shopCapTextReplacements[11] = {packet->gameIndex11, packet->playerIndex11,
                                                  packet->itemIndex11,
                                                  packet->itemClassification11};
        sInstance->shopCapTextReplacements[12] = {packet->gameIndex12, packet->playerIndex12,
                                                  packet->itemIndex12,
                                                  packet->itemClassification12};
        sInstance->shopCapTextReplacements[13] = {packet->gameIndex13, packet->playerIndex13,
                                                  packet->itemIndex13,
                                                  packet->itemClassification13};
        sInstance->shopCapTextReplacements[14] = {packet->gameIndex14, packet->playerIndex14,
                                                  packet->itemIndex14,
                                                  packet->itemClassification14};
        sInstance->shopCapTextReplacements[15] = {packet->gameIndex15, packet->playerIndex15,
                                                  packet->itemIndex15,
                                                  packet->itemClassification15};
        sInstance->shopCapTextReplacements[16] = {packet->gameIndex16, packet->playerIndex16,
                                                  packet->itemIndex16,
                                                  packet->itemClassification16};
        sInstance->shopCapTextReplacements[17] = {packet->gameIndex17, packet->playerIndex17,
                                                  packet->itemIndex17,
                                                  packet->itemClassification17};
        sInstance->shopCapTextReplacements[18] = {packet->gameIndex18, packet->playerIndex18,
                                                  packet->itemIndex18,
                                                  packet->itemClassification18};
        sInstance->shopCapTextReplacements[19] = {packet->gameIndex19, packet->playerIndex19,
                                                  packet->itemIndex19,
                                                  packet->itemClassification19};
        sInstance->shopCapTextReplacements[20] = {packet->gameIndex20, packet->playerIndex20,
                                                  packet->itemIndex20,
                                                  packet->itemClassification20};
        sInstance->shopCapTextReplacements[21] = {packet->gameIndex21, packet->playerIndex21,
                                                  packet->itemIndex21,
                                                  packet->itemClassification21};
        sInstance->shopCapTextReplacements[22] = {packet->gameIndex22, packet->playerIndex22,
                                                  packet->itemIndex22,
                                                  packet->itemClassification22};
        sInstance->shopCapTextReplacements[23] = {packet->gameIndex23, packet->playerIndex23,
                                                  packet->itemIndex23,
                                                  packet->itemClassification23};
        sInstance->shopCapTextReplacements[24] = {packet->gameIndex24, packet->playerIndex24,
                                                  packet->itemIndex24,
                                                  packet->itemClassification24};
        sInstance->shopCapTextReplacements[25] = {packet->gameIndex25, packet->playerIndex25,
                                                  packet->itemIndex25,
                                                  packet->itemClassification25};
        sInstance->shopCapTextReplacements[26] = {packet->gameIndex26, packet->playerIndex26,
                                                  packet->itemIndex26,
                                                  packet->itemClassification26};
        sInstance->shopCapTextReplacements[27] = {packet->gameIndex27, packet->playerIndex27,
                                                  packet->itemIndex27,
                                                  packet->itemClassification27};
        sInstance->shopCapTextReplacements[28] = {packet->gameIndex28, packet->playerIndex28,
                                                  packet->itemIndex28,
                                                  packet->itemClassification28};
        sInstance->shopCapTextReplacements[29] = {packet->gameIndex29, packet->playerIndex29,
                                                  packet->itemIndex29,
                                                  packet->itemClassification29};
        sInstance->shopCapTextReplacements[30] = {packet->gameIndex30, packet->playerIndex30,
                                                  packet->itemIndex30,
                                                  packet->itemClassification30};
        sInstance->shopCapTextReplacements[31] = {packet->gameIndex31, packet->playerIndex31,
                                                  packet->itemIndex31,
                                                  packet->itemClassification31};
        sInstance->shopCapTextReplacements[32] = {packet->gameIndex32, packet->playerIndex32,
                                                  packet->itemIndex32,
                                                  packet->itemClassification32};
        sInstance->shopCapTextReplacements[33] = {packet->gameIndex33, packet->playerIndex33,
                                                  packet->itemIndex33,
                                                  packet->itemClassification33};
        sInstance->shopCapTextReplacements[34] = {packet->gameIndex34, packet->playerIndex34,
                                                  packet->itemIndex34,
                                                  packet->itemClassification34};
        sInstance->shopCapTextReplacements[35] = {packet->gameIndex35, packet->playerIndex35,
                                                  packet->itemIndex35,
                                                  packet->itemClassification35};
        sInstance->shopCapTextReplacements[36] = {packet->gameIndex36, packet->playerIndex36,
                                                  packet->itemIndex36,
                                                  packet->itemClassification36};
        sInstance->shopCapTextReplacements[37] = {packet->gameIndex37, packet->playerIndex37,
                                                  packet->itemIndex37,
                                                  packet->itemClassification37};
        sInstance->shopCapTextReplacements[38] = {packet->gameIndex38, packet->playerIndex38,
                                                  packet->itemIndex38,
                                                  packet->itemClassification38};
        sInstance->shopCapTextReplacements[39] = {packet->gameIndex39, packet->playerIndex39,
                                                  packet->itemIndex39,
                                                  packet->itemClassification39};
        sInstance->shopCapTextReplacements[40] = {packet->gameIndex40, packet->playerIndex40,
                                                  packet->itemIndex40,
                                                  packet->itemClassification40};
        sInstance->shopCapTextReplacements[41] = {packet->gameIndex41, packet->playerIndex41,
                                                  packet->itemIndex41,
                                                  packet->itemClassification41};
        sInstance->shopCapTextReplacements[42] = {packet->gameIndex42, packet->playerIndex42,
                                                  packet->itemIndex42,
                                                  packet->itemClassification42};
        sInstance->shopCapTextReplacements[43] = {packet->gameIndex43, packet->playerIndex43,
                                                  packet->itemIndex43,
                                                  packet->itemClassification43};
    }
    // Cloth
    if (type == 1)
    {
        sInstance->shopClothTextReplacements[0] = {packet->gameIndex0, packet->playerIndex0,
                                                   packet->itemIndex0, packet->itemClassification0};
        sInstance->shopClothTextReplacements[1] = {packet->gameIndex1, packet->playerIndex1,
                                                   packet->itemIndex1, packet->itemClassification1};
        sInstance->shopClothTextReplacements[2] = {packet->gameIndex2, packet->playerIndex2,
                                                   packet->itemIndex2, packet->itemClassification2};
        sInstance->shopClothTextReplacements[3] = {packet->gameIndex3, packet->playerIndex3,
                                                   packet->itemIndex3, packet->itemClassification3};
        sInstance->shopClothTextReplacements[4] = {packet->gameIndex4, packet->playerIndex4,
                                                   packet->itemIndex4, packet->itemClassification4};
        sInstance->shopClothTextReplacements[5] = {packet->gameIndex5, packet->playerIndex5,
                                                   packet->itemIndex5, packet->itemClassification5};
        sInstance->shopClothTextReplacements[6] = {packet->gameIndex6, packet->playerIndex6,
                                                   packet->itemIndex6, packet->itemClassification6};
        sInstance->shopClothTextReplacements[7] = {packet->gameIndex7, packet->playerIndex7,
                                                   packet->itemIndex7, packet->itemClassification7};
        sInstance->shopClothTextReplacements[8] = {packet->gameIndex8, packet->playerIndex8,
                                                   packet->itemIndex8, packet->itemClassification8};
        sInstance->shopClothTextReplacements[9] = {packet->gameIndex9, packet->playerIndex9,
                                                   packet->itemIndex9, packet->itemClassification9};
        sInstance->shopClothTextReplacements[10] = {packet->gameIndex10, packet->playerIndex10,
                                                    packet->itemIndex10,
                                                    packet->itemClassification10};
        sInstance->shopClothTextReplacements[11] = {packet->gameIndex11, packet->playerIndex11,
                                                    packet->itemIndex11,
                                                    packet->itemClassification11};
        sInstance->shopClothTextReplacements[12] = {packet->gameIndex12, packet->playerIndex12,
                                                    packet->itemIndex12,
                                                    packet->itemClassification12};
        sInstance->shopClothTextReplacements[13] = {packet->gameIndex13, packet->playerIndex13,
                                                    packet->itemIndex13,
                                                    packet->itemClassification13};
        sInstance->shopClothTextReplacements[14] = {packet->gameIndex14, packet->playerIndex14,
                                                    packet->itemIndex14,
                                                    packet->itemClassification14};
        sInstance->shopClothTextReplacements[15] = {packet->gameIndex15, packet->playerIndex15,
                                                    packet->itemIndex15,
                                                    packet->itemClassification15};
        sInstance->shopClothTextReplacements[16] = {packet->gameIndex16, packet->playerIndex16,
                                                    packet->itemIndex16,
                                                    packet->itemClassification16};
        sInstance->shopClothTextReplacements[17] = {packet->gameIndex17, packet->playerIndex17,
                                                    packet->itemIndex17,
                                                    packet->itemClassification17};
        sInstance->shopClothTextReplacements[18] = {packet->gameIndex18, packet->playerIndex18,
                                                    packet->itemIndex18,
                                                    packet->itemClassification18};
        sInstance->shopClothTextReplacements[19] = {packet->gameIndex19, packet->playerIndex19,
                                                    packet->itemIndex19,
                                                    packet->itemClassification19};
        sInstance->shopClothTextReplacements[20] = {packet->gameIndex20, packet->playerIndex20,
                                                    packet->itemIndex20,
                                                    packet->itemClassification20};
        sInstance->shopClothTextReplacements[21] = {packet->gameIndex21, packet->playerIndex21,
                                                    packet->itemIndex21,
                                                    packet->itemClassification21};
        sInstance->shopClothTextReplacements[22] = {packet->gameIndex22, packet->playerIndex22,
                                                    packet->itemIndex22,
                                                    packet->itemClassification22};
        sInstance->shopClothTextReplacements[23] = {packet->gameIndex23, packet->playerIndex23,
                                                    packet->itemIndex23,
                                                    packet->itemClassification23};
        sInstance->shopClothTextReplacements[24] = {packet->gameIndex24, packet->playerIndex24,
                                                    packet->itemIndex24,
                                                    packet->itemClassification24};
        sInstance->shopClothTextReplacements[25] = {packet->gameIndex25, packet->playerIndex25,
                                                    packet->itemIndex25,
                                                    packet->itemClassification25};
        sInstance->shopClothTextReplacements[26] = {packet->gameIndex26, packet->playerIndex26,
                                                    packet->itemIndex26,
                                                    packet->itemClassification26};
        sInstance->shopClothTextReplacements[27] = {packet->gameIndex27, packet->playerIndex27,
                                                    packet->itemIndex27,
                                                    packet->itemClassification27};
        sInstance->shopClothTextReplacements[28] = {packet->gameIndex28, packet->playerIndex28,
                                                    packet->itemIndex28,
                                                    packet->itemClassification28};
        sInstance->shopClothTextReplacements[29] = {packet->gameIndex29, packet->playerIndex29,
                                                    packet->itemIndex29,
                                                    packet->itemClassification29};
        sInstance->shopClothTextReplacements[30] = {packet->gameIndex30, packet->playerIndex30,
                                                    packet->itemIndex30,
                                                    packet->itemClassification30};
        sInstance->shopClothTextReplacements[31] = {packet->gameIndex31, packet->playerIndex31,
                                                    packet->itemIndex31,
                                                    packet->itemClassification31};
        sInstance->shopClothTextReplacements[32] = {packet->gameIndex32, packet->playerIndex32,
                                                    packet->itemIndex32,
                                                    packet->itemClassification32};
        sInstance->shopClothTextReplacements[33] = {packet->gameIndex33, packet->playerIndex33,
                                                    packet->itemIndex33,
                                                    packet->itemClassification33};
        sInstance->shopClothTextReplacements[34] = {packet->gameIndex34, packet->playerIndex34,
                                                    packet->itemIndex34,
                                                    packet->itemClassification34};
        sInstance->shopClothTextReplacements[35] = {packet->gameIndex35, packet->playerIndex35,
                                                    packet->itemIndex35,
                                                    packet->itemClassification35};
        sInstance->shopClothTextReplacements[36] = {packet->gameIndex36, packet->playerIndex36,
                                                    packet->itemIndex36,
                                                    packet->itemClassification36};
        sInstance->shopClothTextReplacements[37] = {packet->gameIndex37, packet->playerIndex37,
                                                    packet->itemIndex37,
                                                    packet->itemClassification37};
        sInstance->shopClothTextReplacements[38] = {packet->gameIndex38, packet->playerIndex38,
                                                    packet->itemIndex38,
                                                    packet->itemClassification38};
        sInstance->shopClothTextReplacements[39] = {packet->gameIndex39, packet->playerIndex39,
                                                    packet->itemIndex39,
                                                    packet->itemClassification39};
        sInstance->shopClothTextReplacements[40] = {packet->gameIndex40, packet->playerIndex40,
                                                    packet->itemIndex40,
                                                    packet->itemClassification40};
        sInstance->shopClothTextReplacements[41] = {packet->gameIndex41, packet->playerIndex41,
                                                    packet->itemIndex41,
                                                    packet->itemClassification41};
        sInstance->shopClothTextReplacements[42] = {packet->gameIndex42, packet->playerIndex42,
                                                    packet->itemIndex42,
                                                    packet->itemClassification42};
        sInstance->shopClothTextReplacements[43] = {packet->gameIndex43, packet->playerIndex43,
                                                    packet->itemIndex43,
                                                    packet->itemClassification43};
    }
    // Sticker
    if (type == 2)
    {
        sInstance->shopStickerTextReplacements[0] = {packet->gameIndex0, packet->playerIndex0,
                                                     packet->itemIndex0,
                                                     packet->itemClassification0};
        sInstance->shopStickerTextReplacements[1] = {packet->gameIndex1, packet->playerIndex1,
                                                     packet->itemIndex1,
                                                     packet->itemClassification1};
        sInstance->shopStickerTextReplacements[2] = {packet->gameIndex2, packet->playerIndex2,
                                                     packet->itemIndex2,
                                                     packet->itemClassification2};
        sInstance->shopStickerTextReplacements[3] = {packet->gameIndex3, packet->playerIndex3,
                                                     packet->itemIndex3,
                                                     packet->itemClassification3};
        sInstance->shopStickerTextReplacements[4] = {packet->gameIndex4, packet->playerIndex4,
                                                     packet->itemIndex4,
                                                     packet->itemClassification4};
        sInstance->shopStickerTextReplacements[5] = {packet->gameIndex5, packet->playerIndex5,
                                                     packet->itemIndex5,
                                                     packet->itemClassification5};
        sInstance->shopStickerTextReplacements[6] = {packet->gameIndex6, packet->playerIndex6,
                                                     packet->itemIndex6,
                                                     packet->itemClassification6};
        sInstance->shopStickerTextReplacements[7] = {packet->gameIndex7, packet->playerIndex7,
                                                     packet->itemIndex7,
                                                     packet->itemClassification7};
        sInstance->shopStickerTextReplacements[8] = {packet->gameIndex8, packet->playerIndex8,
                                                     packet->itemIndex8,
                                                     packet->itemClassification8};
        sInstance->shopStickerTextReplacements[9] = {packet->gameIndex9, packet->playerIndex9,
                                                     packet->itemIndex9,
                                                     packet->itemClassification9};
        sInstance->shopStickerTextReplacements[10] = {packet->gameIndex10, packet->playerIndex10,
                                                      packet->itemIndex10,
                                                      packet->itemClassification10};
        sInstance->shopStickerTextReplacements[11] = {packet->gameIndex11, packet->playerIndex11,
                                                      packet->itemIndex11,
                                                      packet->itemClassification11};
        sInstance->shopStickerTextReplacements[12] = {packet->gameIndex12, packet->playerIndex12,
                                                      packet->itemIndex12,
                                                      packet->itemClassification12};
        sInstance->shopStickerTextReplacements[13] = {packet->gameIndex13, packet->playerIndex13,
                                                      packet->itemIndex13,
                                                      packet->itemClassification13};
        sInstance->shopStickerTextReplacements[14] = {packet->gameIndex14, packet->playerIndex14,
                                                      packet->itemIndex14,
                                                      packet->itemClassification14};
        sInstance->shopStickerTextReplacements[15] = {packet->gameIndex15, packet->playerIndex15,
                                                      packet->itemIndex15,
                                                      packet->itemClassification15};
        sInstance->shopStickerTextReplacements[16] = {packet->gameIndex16, packet->playerIndex16,
                                                      packet->itemIndex16,
                                                      packet->itemClassification16};
    }
    // Gift
    if (type == 3)
    {
        sInstance->shopGiftTextReplacements[0] = {packet->gameIndex0, packet->playerIndex0,
                                                  packet->itemIndex0, packet->itemClassification0};
        sInstance->shopGiftTextReplacements[1] = {packet->gameIndex1, packet->playerIndex1,
                                                  packet->itemIndex1, packet->itemClassification1};
        sInstance->shopGiftTextReplacements[2] = {packet->gameIndex2, packet->playerIndex2,
                                                  packet->itemIndex2, packet->itemClassification2};
        sInstance->shopGiftTextReplacements[3] = {packet->gameIndex3, packet->playerIndex3,
                                                  packet->itemIndex3, packet->itemClassification3};
        sInstance->shopGiftTextReplacements[4] = {packet->gameIndex4, packet->playerIndex4,
                                                  packet->itemIndex4, packet->itemClassification4};
        sInstance->shopGiftTextReplacements[5] = {packet->gameIndex5, packet->playerIndex5,
                                                  packet->itemIndex5, packet->itemClassification5};
        sInstance->shopGiftTextReplacements[6] = {packet->gameIndex6, packet->playerIndex6,
                                                  packet->itemIndex6, packet->itemClassification6};
        sInstance->shopGiftTextReplacements[7] = {packet->gameIndex7, packet->playerIndex7,
                                                  packet->itemIndex7, packet->itemClassification7};
        sInstance->shopGiftTextReplacements[8] = {packet->gameIndex8, packet->playerIndex8,
                                                  packet->itemIndex8, packet->itemClassification8};
        sInstance->shopGiftTextReplacements[9] = {packet->gameIndex9, packet->playerIndex9,
                                                  packet->itemIndex9, packet->itemClassification9};
        sInstance->shopGiftTextReplacements[10] = {packet->gameIndex10, packet->playerIndex10,
                                                   packet->itemIndex10,
                                                   packet->itemClassification10};
        sInstance->shopGiftTextReplacements[11] = {packet->gameIndex11, packet->playerIndex11,
                                                   packet->itemIndex11,
                                                   packet->itemClassification11};
        sInstance->shopGiftTextReplacements[12] = {packet->gameIndex12, packet->playerIndex12,
                                                   packet->itemIndex12,
                                                   packet->itemClassification12};
        sInstance->shopGiftTextReplacements[13] = {packet->gameIndex13, packet->playerIndex13,
                                                   packet->itemIndex13,
                                                   packet->itemClassification13};
        sInstance->shopGiftTextReplacements[14] = {packet->gameIndex14, packet->playerIndex14,
                                                   packet->itemIndex14,
                                                   packet->itemClassification14};
        sInstance->shopGiftTextReplacements[15] = {packet->gameIndex15, packet->playerIndex15,
                                                   packet->itemIndex15,
                                                   packet->itemClassification15};
        sInstance->shopGiftTextReplacements[16] = {packet->gameIndex16, packet->playerIndex16,
                                                   packet->itemIndex16,
                                                   packet->itemClassification16};
        sInstance->shopGiftTextReplacements[17] = {packet->gameIndex17, packet->playerIndex17,
                                                   packet->itemIndex17,
                                                   packet->itemClassification17};
        sInstance->shopGiftTextReplacements[18] = {packet->gameIndex18, packet->playerIndex18,
                                                   packet->itemIndex18,
                                                   packet->itemClassification18};
        sInstance->shopGiftTextReplacements[19] = {packet->gameIndex19, packet->playerIndex19,
                                                   packet->itemIndex19,
                                                   packet->itemClassification19};
        sInstance->shopGiftTextReplacements[20] = {packet->gameIndex20, packet->playerIndex20,
                                                   packet->itemIndex20,
                                                   packet->itemClassification20};
        sInstance->shopGiftTextReplacements[21] = {packet->gameIndex21, packet->playerIndex21,
                                                   packet->itemIndex21,
                                                   packet->itemClassification21};
        sInstance->shopGiftTextReplacements[22] = {packet->gameIndex22, packet->playerIndex22,
                                                   packet->itemIndex22,
                                                   packet->itemClassification22};
        sInstance->shopGiftTextReplacements[23] = {packet->gameIndex23, packet->playerIndex23,
                                                   packet->itemIndex23,
                                                   packet->itemClassification23};
        sInstance->shopGiftTextReplacements[24] = {packet->gameIndex24, packet->playerIndex24,
                                                   packet->itemIndex24,
                                                   packet->itemClassification24};
        sInstance->shopGiftTextReplacements[25] = {packet->gameIndex25, packet->playerIndex25,
                                                   packet->itemIndex25,
                                                   packet->itemClassification25};
    }
    // Moon
    if (type == 4)
    {
        sInstance->shopMoonTextReplacements[0] = {packet->gameIndex0, packet->playerIndex0,
                                                  packet->itemIndex0, packet->itemClassification0};
        sInstance->shopMoonTextReplacements[1] = {packet->gameIndex1, packet->playerIndex1,
                                                  packet->itemIndex1, packet->itemClassification1};
        sInstance->shopMoonTextReplacements[2] = {packet->gameIndex2, packet->playerIndex2,
                                                  packet->itemIndex2, packet->itemClassification2};
        sInstance->shopMoonTextReplacements[3] = {packet->gameIndex3, packet->playerIndex3,
                                                  packet->itemIndex3, packet->itemClassification3};
        sInstance->shopMoonTextReplacements[4] = {packet->gameIndex4, packet->playerIndex4,
                                                  packet->itemIndex4, packet->itemClassification4};
        sInstance->shopMoonTextReplacements[5] = {packet->gameIndex5, packet->playerIndex5,
                                                  packet->itemIndex5, packet->itemClassification5};
        sInstance->shopMoonTextReplacements[6] = {packet->gameIndex6, packet->playerIndex6,
                                                  packet->itemIndex6, packet->itemClassification6};
        sInstance->shopMoonTextReplacements[7] = {packet->gameIndex7, packet->playerIndex7,
                                                  packet->itemIndex7, packet->itemClassification7};
        sInstance->shopMoonTextReplacements[8] = {packet->gameIndex8, packet->playerIndex8,
                                                  packet->itemIndex8, packet->itemClassification8};
        sInstance->shopMoonTextReplacements[9] = {packet->gameIndex9, packet->playerIndex9,
                                                  packet->itemIndex9, packet->itemClassification9};
        sInstance->shopMoonTextReplacements[10] = {packet->gameIndex10, packet->playerIndex10,
                                                   packet->itemIndex10,
                                                   packet->itemClassification10};
        sInstance->shopMoonTextReplacements[11] = {packet->gameIndex11, packet->playerIndex11,
                                                   packet->itemIndex11,
                                                   packet->itemClassification11};
        sInstance->shopMoonTextReplacements[12] = {packet->gameIndex12, packet->playerIndex12,
                                                   packet->itemIndex12,
                                                   packet->itemClassification12};
    }

}

const char* Client::getShineReplacementText() 
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return "";
    }

    GameDataHolderAccessor accessor(sInstance->mCurStageScene);

    Shine* curShine = sInstance->recentShine;

    GameDataFile::HintInfo* info =
        &accessor.mData->mGameDataFile->mShineHintList[curShine->mShineIdx];

    shineReplaceText curReplaceText;

    if (info->mUniqueID == 0) {
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "CapWorldHomeStage") == 0) {
            curReplaceText = sInstance->shineTextReplacements[98];
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "SandWorldHomeStage") == 0) {
            curReplaceText = sInstance->shineTextReplacements[98];
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "LakeWorldHomeStage") == 0) {
            curReplaceText = sInstance->shineTextReplacements[98];
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "ForestWorldHomeStage") == 0) {
            curReplaceText = sInstance->shineTextReplacements[98];
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "CityWorldHomeStage") == 0) {
            curReplaceText = sInstance->shineTextReplacements[98];
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "SnowWorldHomeStage") == 0) {
            curReplaceText = sInstance->shineTextReplacements[98];
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "SeaWorldHomeStage") == 0) {
            curReplaceText = sInstance->shineTextReplacements[98];
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "LavaWorldHomeStage") == 0) {
            curReplaceText = sInstance->shineTextReplacements[98];
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "SkyWorldHomeStage") == 0) {
            curReplaceText = sInstance->shineTextReplacements[99];
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "MoonWorldHomeStage") == 0) {
            curReplaceText = sInstance->shineTextReplacements[98];
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "PeachWorldHomeStage") == 0) {
            curReplaceText = sInstance->shineTextReplacements[98];
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "Special1WorldHomeStage") == 0) {
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(accessor), "WaterfallWorldHomeStage") == 0) {
                curReplaceText = sInstance->shineTextReplacements[99];
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(accessor), "LakeWorldHomeStage") == 0) {
                curReplaceText = sInstance->shineTextReplacements[99];
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(accessor), "CloudWorldHomeStage") == 0) {
                curReplaceText = sInstance->shineTextReplacements[99];
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(accessor), "ClashWorldHomeStage") == 0) {
                curReplaceText = sInstance->shineTextReplacements[99];
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(accessor), "CityWorldHomeStage") == 0) {
                curReplaceText = sInstance->shineTextReplacements[99];
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(accessor), "SnowWorldHomeStage") == 0) {
                curReplaceText = sInstance->shineTextReplacements[99];
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(accessor), "SeaWorldHomeStage") == 0) {
                curReplaceText = sInstance->shineTextReplacements[99];
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(accessor), "LavaWorldHomeStage") == 0) {
                curReplaceText = sInstance->shineTextReplacements[99];
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(accessor), "BossRaidWorldHomeStage") == 0) {
                curReplaceText = sInstance->shineTextReplacements[99];
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(accessor), "PeachWorldHomeStage") == 0) {
                curReplaceText = sInstance->shineTextReplacements[99];
            }
        }
    } else {
        curReplaceText = sInstance->shineTextReplacements[info->mHintIdx];
    }

    //setMessage(1, intToCstr(info->mHintIdx));

    if (curReplaceText.shineItemNameIndex == 255)
    {
        appendMessage("\x1b[31mInvalid shine item name index\x1b[0m");
        return sInstance->recentShine->curShineInfo->mShineLabel.cstr();
    } else {
        return sInstance->shineItemNames[curReplaceText.shineItemNameIndex].cstr();
    }
    
}

int Client::getShineColor(Shine* curShine)
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return 99;
    }

    GameDataHolderAccessor accessor(sInstance->mCurStageScene);

    GameDataFile::HintInfo* info =
        &accessor.mData->mGameDataFile->mShineHintList[curShine->mShineIdx];

    
    // Hint arts Uid is 0 on the moon object in the other world.
    // Stage name in the shine info is still the kingdom the hint art comes from.
    if (info->mUniqueID == 0) {
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "CapWorldHomeStage") == 0) {
            return static_cast<int>(sInstance->shineColors[1086]);
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "SandWorldHomeStage") == 0) {
            return static_cast<int>(sInstance->shineColors[1096]);
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "LakeWorldHomeStage") == 0) {
            return static_cast<int>(sInstance->shineColors[1094]);
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "ForestWorldHomeStage") == 0) {
            return static_cast<int>(sInstance->shineColors[1089]);
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "CityWorldHomeStage") == 0) {
            return static_cast<int>(sInstance->shineColors[1088]);
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "SnowWorldHomeStage") == 0) {
            return static_cast<int>(sInstance->shineColors[1087]);
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "SeaWorldHomeStage") == 0) {
            return static_cast<int>(sInstance->shineColors[1095]);
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "LavaWorldHomeStage") == 0) {
            return static_cast<int>(sInstance->shineColors[1090]);
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "SkyWorldHomeStage") == 0) {
            return static_cast<int>(sInstance->shineColors[1091]);
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "MoonWorldHomeStage") == 0) {
            return static_cast<int>(sInstance->shineColors[1165]);
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "PeachWorldHomeStage") == 0) {
            return static_cast<int>(sInstance->shineColors[1152]);
        }
        if (strcmp(curShine->curShineInfo->stageName.cstr(), "Special1WorldHomeStage") == 0) {
            // Add conditions for other Dark Side hint arts
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(accessor),
                       "WaterfallWorldHomeStage") == 0) {
                return static_cast<int>(sInstance->shineColors[1132]);
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(accessor),
                       "LakeWorldHomeStage") == 0) {
                return static_cast<int>(sInstance->shineColors[1128]);
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(accessor),
                       "CloudWorldHomeStage") == 0) {
                return static_cast<int>(sInstance->shineColors[1124]);
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(accessor),
                       "ClashWorldHomeStage") == 0) {
                return static_cast<int>(sInstance->shineColors[1126]);
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(accessor),
                       "CityWorldHomeStage") == 0) {
                return static_cast<int>(sInstance->shineColors[1130]);
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(accessor),
                       "SnowWorldHomeStage") == 0) {
                return static_cast<int>(sInstance->shineColors[1129]);
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(accessor),
                       "SeaWorldHomeStage") == 0) {
                return static_cast<int>(sInstance->shineColors[1127]);
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(accessor),
                       "LavaWorldHomeStage") == 0) {
                return static_cast<int>(sInstance->shineColors[1123]);
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(accessor),
                       "BossRaidWorldHomeStage") == 0) {
                return static_cast<int>(sInstance->shineColors[1125]);
            }
            if (strcmp(GameDataFunction::tryGetCurrentMainStageName(accessor),
                       "PeachWorldHomeStage") == 0) {
                return static_cast<int>(sInstance->shineColors[1131]);
            }
            
        }
    } else {
        /*sead::FixedSafeString<40> shineData;
        shineData = "";
        shineData.append("Uid: ");
        shineData.append(intToCstr(info->mUniqueID));
        shineData.append(" Color: ");
        shineData.append(intToCstr(sInstance->shineColors[info->mUniqueID]));

        setMessage(1, shineData.cstr());
        shineData = "";
        shineData.append("Uid: ");
        shineData.append(intToCstr(1145));
        shineData.append(" Color: ");
        shineData.append(intToCstr(sInstance->shineColors[1145]));

        setMessage(2, shineData.cstr());*/
        return static_cast<int>(sInstance->shineColors[info->mUniqueID]);
    }
    return 99;
}

const char16_t* Client::getShopReplacementText(const char* fileName, const char* key) 
{
    if (!sInstance) {
        Logger::log("Static Instance is Null!\n");
        return u"";
    }

    sead::WFixedSafeString<200> message;
    message = message.cEmptyString;
    bool isExplain = false;
    sead::FixedSafeString<40> convert;
    convert = "";
    convert.append(key);
    if (convert.calcLength() != convert.removeSuffix("_Explain")) {
        isExplain = true;
    }
    shopReplaceText curItem = {255,255,255,255};

    if (strcmp("ItemCap", fileName) == 0)
    {
        curItem = sInstance->shopCapTextReplacements[getIndexCostumeList(convert.cstr()) - 1];
    }
    else if (strcmp("ItemCloth", fileName) == 0) 
    {
        curItem = sInstance->shopClothTextReplacements[getIndexCostumeList(convert.cstr()) - 1];
    }
    else if (strcmp("ItemSticker", fileName) == 0) 
    {
        curItem = sInstance->shopStickerTextReplacements[getIndexStickerList(convert.cstr())];
    }
    else if (strcmp("ItemGift", fileName) == 0) 
    {
        curItem = sInstance->shopGiftTextReplacements[getIndexSouvenirList(convert.cstr())];
    }
    else if (strcmp("ItemMoon", fileName) == 0) 
    {
        // Find out key for each kingdom as still is unknown
        curItem = sInstance->shopMoonTextReplacements[getIndexMoonItemList(convert.cstr())];
    } else {
        // Not included items like Life Up Hearts
        return u"";
    }

    if (curItem.gameIndex == 254)
    {
        //setMessage(1, "No Item Data Received.");
    }

    if (isExplain)
    {
        message.append(u"Comes from the world of ");
        //if (sInstance->apGameNames[curItem.gameIndex].isEmpty()) {
            message.append(sInstance->apGameNames[curItem.gameIndex].cstr());
        //} else {
            //message.append(u"Missing Game");
       // }

        message.append(u".\nSeems to belong to ");
        //if (sInstance->apSlotNames[curItem.slotIndex].isEmpty()) {
            message.append(sInstance->apSlotNames[curItem.slotIndex].cstr());
        //} else {
            //message.append(u"Missing Slot Name");
        //}
        message.append(u".\n");
        if (curItem.itemClassification == 0) {
            message.append(u"It looks like junk, but may as well ask...");
        } else if (curItem.itemClassification == 0b0010) {
            message.append(u"It looks useful.");
           
        } else if (curItem.itemClassification == 254) {
            message.append(u"Error or Not in the Item Pool.");
        }
        else {
            message.append(u"It looks really important!");
        }
    } else {
        //if (sInstance->apSlotNames[curItem.slotIndex].isEmpty()) {
            message.append(sInstance->apItemNames[curItem.apItemNameIndex].cstr());
        //} else {
            //message.append(u"Missing Item Name");
        //}
    }

    return message.cstr();
}

/**
 * @brief 
 * 
 * @param puppet 
 * @return true 
 * @return false 
 */
bool Client::tryAddPuppet(PuppetActor *puppet) {
    if(sInstance) {
        return sInstance->mPuppetHolder->tryRegisterPuppet(puppet);
    }else {
        return false;
    }
}

/**
 * @brief 
 * 
 * @param puppet 
 * @return true 
 * @return false 
 */
bool Client::tryAddDebugPuppet(PuppetActor *puppet) {
    if(sInstance) {
        return sInstance->mPuppetHolder->tryRegisterDebugPuppet(puppet);
    }else {
        return false;
    }
}

/**
 * @brief 
 * 
 * @param idx 
 * @return PuppetActor* 
 */
PuppetActor *Client::getPuppet(int idx) {
    if(sInstance) {
        return sInstance->mPuppetHolder->getPuppetActor(idx);
    }else {
        return nullptr;
    }
}

/**
 * @brief 
 * 
 * @return PuppetInfo* 
 */
PuppetInfo *Client::getLatestInfo() {
    if(sInstance) {
        return Client::getPuppetInfo(sInstance->mPuppetHolder->getSize() - 1);
    }else {
        return nullptr;
    }
}

/**
 * @brief 
 * 
 * @param idx 
 * @return PuppetInfo* 
 */
PuppetInfo *Client::getPuppetInfo(int idx) {
    if(sInstance) {
        // unsafe get
        PuppetInfo *curInfo = sInstance->mPuppetInfoArr[idx];

        if (!curInfo) {
            Logger::log("Attempting to Access Puppet Out of Bounds! Value: %d\n", idx);
            return nullptr;
        }

        return curInfo;
    }else {
        return nullptr;
    }
}

/**
 * @brief 
 * 
 */
void Client::resetCollectedShines() {
    collectedShineCount = 0;
    curCollectedShines.fill(-1);
}

/**
 * @brief 
 * 
 * @param shineId 
 */
void Client::removeShine(int shineId) {
    for (size_t i = 0; i < curCollectedShines.size(); i++)
    {
        if(curCollectedShines[i] == shineId) {
            curCollectedShines[i] = -1;
            collectedShineCount--;
        }
    }
}

/**
 * @brief 
 * 
 * @return true 
 * @return false 
 */
bool Client::isNeedUpdateShines() {
    return sInstance ? sInstance->collectedShineCount > 0 : false;
}

/**
 * @brief 
 * 
 */
void Client::updateShines() {
    if (!sInstance) {
        Logger::log("Client Null!\n");
        return;
    }

    // skip shine sync if player is in cap kingdom scenario zero (very start of the game)
    if (sInstance->mStageName == "CapWorldHomeStage" && (sInstance->mScenario == 0 || sInstance->mScenario == 1)) {
        return;
    }

    GameDataHolderAccessor accessor(sInstance->mCurStageScene);
    
    for (size_t i = 0; i < sInstance->getCollectedShinesCount(); i++)
    {
        int shineID = sInstance->getShineID(i);

        if(shineID < 0) continue;

        Logger::log("Shine UID: %d\n", shineID);

        GameDataFile::HintInfo* shineInfo = CustomGameDataFunction::getHintInfoByUniqueID(accessor, shineID);

        if (shineInfo) {
            if (!GameDataFunction::isGotShine(accessor, shineInfo->mStageName.cstr(), shineInfo->mObjId.cstr())) {

                Shine* stageShine = findStageShine(shineID);
                
                if (stageShine) {

                    if (al::isDead(stageShine)) {
                        stageShine->makeActorAlive();
                    }
                    
                    
                    //stageShine->getDirect();
                    //stageShine->onSwitchGet();
                }

                accessor.mData->mGameDataFile->setGotShine(shineInfo);
            }
        }
    }
    
    sInstance->resetCollectedShines();
    startShineCount();
}

/**
 * @brief
 *
 */
void Client::updateChatMessages(ArchipelagoChatMessage* packet)
{
    if (!sInstance) {
        Logger::log("Client Null!\n");
        return;
    }

    if (strlen(packet->message1) != 0)
        sInstance->appendMessage(packet->message1);
    else if (strlen(packet->message2) != 0)
        sInstance->appendMessage(packet->message2);
    else
        sInstance->appendMessage(packet->message3);
}

void Client::updateSlotData(SlotData* packet) {
    if (!sInstance) {
        Logger::log("Client Null!\n");
        return;
    }
    sInstance->worldPayCounts[1] = packet->cascade;
    sInstance->worldPayCounts[2] = packet->sand;
    sInstance->worldPayCounts[3] = packet->wooded;
    sInstance->worldPayCounts[4] = packet->lake;
    sInstance->worldPayCounts[6] = packet->lost;
    sInstance->worldPayCounts[7] = packet->metro;
    sInstance->worldPayCounts[8] = packet->seaside;
    sInstance->worldPayCounts[9] = packet->snow;
    sInstance->worldPayCounts[10] = packet->luncheon;
    sInstance->worldPayCounts[11] = packet->ruined;
    sInstance->worldPayCounts[12] = packet->bowser;
    sInstance->worldPayCounts[15] = packet->dark;
    sInstance->worldPayCounts[16] = packet->darker;
    sInstance->regionals = packet->regionals;
    sInstance->captures = packet->captures;

    sInstance->numApGames = 0;
    sInstance->numApSlots = 0;
    sInstance->numApItems = 0;
}

void Client::updateWorlds(UnlockWorld* packet)
{
    if (!sInstance) {
        Logger::log("Client Null!\n");
        return;
    }

    GameDataHolderAccessor accessor(sInstance->mCurStageScene);
    GameDataFunction::unlockWorld(accessor, packet->worldID);
}

/**
 * @brief 
 * 
 */
void Client::update() {
    if (sInstance) {
        
        sInstance->mPuppetHolder->update();

        if (isNeedUpdateShines()) {
            updateShines();
        }

        GameModeManager::instance()->update();
    }
}

/**
 * @brief 
 * 
 */
void Client::clearArrays() {
    if(sInstance) {
        sInstance->mPuppetHolder->clearPuppets();
        sInstance->mShineArray.clear();

    }
}

/**
 * @brief 
 * 
 * @return PuppetInfo* 
 */
PuppetInfo *Client::getDebugPuppetInfo() {
    if(sInstance) {
        return &sInstance->mDebugPuppetInfo;
    }else {
        return nullptr;
    }
}

/**
 * @brief 
 * 
 * @return PuppetActor* 
 */
PuppetActor *Client::getDebugPuppet() {
    if(sInstance) {
        return sInstance->mPuppetHolder->getDebugPuppet();
    }else {
        return nullptr;
    }
}

/**
 * @brief 
 * 
 * @return Keyboard* 
 */
Keyboard* Client::getKeyboard() {
    if (sInstance) {
        return sInstance->mKeyboard;
    }
    return nullptr;
}

/**
 * @brief 
 * 
 * @return const char* 
 */
const char* Client::getCurrentIP() {
    if (sInstance) {
        return sInstance->mServerIP.cstr();
    }
    return nullptr;
}

/**
 * @brief 
 * 
 * @return const int 
 */
const int Client::getCurrentPort() {
    if (sInstance) {
        return sInstance->mServerPort;
    }
    return -1;
}

/**
 * @brief sets server IP to supplied string, used specifically for loading IP from the save file.
 * 
 * @param ip 
 */
void Client::setLastUsedIP(const char* ip) {
    if (sInstance) {
        sInstance->mServerIP = ip;
    }
}

/**
 * @brief sets server port to supplied string, used specifically for loading port from the save file.
 * 
 * @param port 
 */
void Client::setLastUsedPort(const int port) {
    if (sInstance) {
        sInstance->mServerPort = port;
    }
}

/**
 * @brief creates new scene info and copies supplied info to the new info, as well as stores a const ptr to the current stage scene.
 * 
 * @param initInfo 
 * @param stageScene 
 */
void Client::setSceneInfo(const al::ActorInitInfo& initInfo, const StageScene *stageScene) {

    if (!sInstance) {
        Logger::log("Client Null!\n");
        return;
    }

    sInstance->mSceneInfo = new al::ActorSceneInfo();

    memcpy(sInstance->mSceneInfo, &initInfo.mActorSceneInfo, sizeof(al::ActorSceneInfo));

    sInstance->mCurStageScene = stageScene;
}

/**
 * @brief stores shine pointer supplied into a ptr array if space is available, and shine is not collected.
 * 
 * @param shine 
 * @return true if shine was able to be successfully stored
 * @return false if shine is already collected, or ptr array is full
 */
bool Client::tryRegisterShine(Shine* shine) {
    if (sInstance) {
        if (!sInstance->mShineArray.isFull()) {
            if (!shine->isGot()) {
                sInstance->mShineArray.pushBack(shine);
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief finds the actor pointer stored in the shine ptr array based off shine ID
 * 
 * @param shineID Unique ID used for shine actor
 * @return Shine* if shine ptr array contains actor with supplied shine ID.
 */
Shine* Client::findStageShine(int shineID) {
    if (sInstance) {
        for (int i = 0; i < sInstance->mShineArray.size(); i++) {

            Shine* curShine = sInstance->mShineArray[i];

            if (curShine) {

                auto hintInfo =
                    CustomGameDataFunction::getHintInfoByIndex(curShine, curShine->mShineIdx);
                
                if (hintInfo->mUniqueID == shineID) {
                    return curShine;
                }
            }
        }
    }
    return nullptr;
}
