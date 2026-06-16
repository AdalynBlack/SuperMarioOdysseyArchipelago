#include "debug/DebugMenu.h"

using namespace debug;

int DebugMenu::pageIndex = 0;
int DebugMenu::puppetIndex = 0;
bool DebugMenu::isDisableMusic = false;

void DebugMenu::updateInputs(StageScene *stageScene) {
    if (al::isPadHoldZR(-1))
        handleInputZR(stageScene);

    if (al::isPadHoldZL(-1))
        handleInputZL(stageScene);

    if (al::isPadHoldL(-1))
        handleInputL(stageScene);
    
    if (isDisableMusic)
        if (al::isPlayingBgm(stageScene))
            al::stopAllBgm(stageScene, 0);
}

void DebugMenu::handleInputZR(StageScene *stageScene) {
    if (al::isPadTriggerUp(-1)) DebugState::debugMode = !DebugState::debugMode;
    if (al::isPadTriggerLeft(-1)) pageIndex--;
    if (al::isPadTriggerRight(-1)) pageIndex++;

    pageIndex = al::modi(pageIndex, maxPages);
}

void DebugMenu::handleInputZL(StageScene *stageScene) {
    if (!DebugState::debugMode)
        return;

    if (al::isPadTriggerLeft(-1)) puppetIndex--;
    if (al::isPadTriggerRight(-1)) puppetIndex++;

    if(puppetIndex < 0) {
        puppetIndex = Client::getMaxPlayerCount() - 2;
    }
    if (puppetIndex >= Client::getMaxPlayerCount() - 1)
        puppetIndex = 0;
}

void DebugMenu::handleInputL(StageScene *stageScene) {
    if (al::isPadTriggerLeft(-1)) GameModeManager::instance()->toggleActive();

    if (al::isPadTriggerUp(-1)) {
        isDisableMusic = !isDisableMusic;
        Client::appendMessage("\x1b[%dmMusic %s\x1b[0m", isDisableMusic ? 31 : 32,isDisableMusic ? "Disabled" : "Enabled");
    }
}

bool DebugMenu::draw(sead::Viewport *viewport, HakoniwaSequence *sequence, bool isInGame, int dispHeight) {
    if (!DebugState::debugMode)
        return false;

    sead::TextWriter *gTextWriter = ImmOverlayMenu::sInstance->gTextWriter;

    gTextWriter->printf("FPS: %d\n", static_cast<int>(round(Application::sInstance->mFramework->calcFps())));

    drawBackground();

    gTextWriter->setCursorFromTopLeft(sead::Vector2f(10.f, (dispHeight / 3) + 30.f));
    gTextWriter->setScaleFromFontHeight(20.f);
    gTextWriter->beginDraw();

    sead::Heap* clientHeap = Client::getClientHeap();
    sead::Heap *gmHeap = GameModeManager::instance()->getHeap();

    if (clientHeap) {
        gTextWriter->printf("Client Heap Free Size: %f/%f\n", clientHeap->getFreeSize() * 0.001f, clientHeap->getSize() * 0.001f);
        gTextWriter->printf("Gamemode Heap Free Size: %f/%f\n", gmHeap->getFreeSize() * 0.001f, gmHeap->getSize()* 0.001f);
    }

    gTextWriter->printf("Client Socket Connection Status: %s\n", Client::instance()->mSocket->getStateChar());
    gTextWriter->printf("Udp socket status: %s\n", Client::instance()->mSocket->getUdpStateChar());
    gTextWriter->printf("nn::socket::GetLastErrno: 0x%x\n", Client::instance()->mSocket->socket_errno);
    gTextWriter->printf("Connected Players: %d/%d\n", Client::getConnectCount() + 1, Client::getMaxPlayerCount());
    
    gTextWriter->printf("Send Queue Count: %d/%d\n", Client::instance()->mSocket->getSendCount(), Client::instance()->mSocket->getSendMaxCount());
    gTextWriter->printf("Recv Queue Count: %d/%d\n", Client::instance()->mSocket->getRecvCount(), Client::instance()->mSocket->getRecvMaxCount());

    if (!sequence)
        return true;
    if (!sequence->curScene)
        return true;
    if (!isInGame)
        return true;

    sead::LookAtCamera *cam = al::getLookAtCamera(sequence->curScene, 0);
    sead::Projection *projection = al::getProjectionSead(sequence->curScene, 0);
    PlayerActorBase *playerBase = rs::getPlayerActor(sequence->curScene);

    PuppetActor *puppet = Client::getPuppet(puppetIndex);
    PuppetActor *debugPuppet = Client::getDebugPuppet();
    if (debugPuppet)
        puppet = debugPuppet;

    gTextWriter->printf("----------- Page %d ------------\n", pageIndex);
    switch (pageIndex)
    {
    case 0:
        {
            gTextWriter->printf("Puppet Index: %d\n", puppetIndex);

            if (!puppet)
                break;

            sead::PrimitiveRenderer *renderer = sead::PrimitiveRenderer::instance();
            renderer->setDrawContext(ImmOverlayMenu::sInstance->context);
            renderer->setCamera(*cam);
            renderer->setProjection(*projection);

            renderer->begin();
            renderer->drawSphere4x8(puppet->getInfo()->playerPos, 20, sead::Color4f(1.f, 0.f, 0.f, 0.25f));
            renderer->drawSphere4x8(al::getTrans(puppet), 20, sead::Color4f(0.f, 0.f, 1.f, 0.25f));
            renderer->setModelMatrix(sead::Matrix34f::ident);
            renderer->end();

            PuppetInfo* puppetInfo = puppet->getInfo();
            if (!puppetInfo)
                break;

            gTextWriter->printf("Player Name: %s\n", puppetInfo->puppetName);
            gTextWriter->printf("Connection Status: %s\n", puppetInfo->isConnected ? "Online" : "Offline");
            gTextWriter->printf("Is in Same Stage: %s\n", puppetInfo->isInSameStage ? "True" : "False");
            gTextWriter->printf("Is in Capture: %s\n", puppetInfo->isCaptured ? "True" : "False");
            gTextWriter->printf("Is Puppet Tagged: %s\n", BTOC(puppetInfo->isIt));
            gTextWriter->printf("Puppet Stage: %s\n", puppetInfo->stageName);
            gTextWriter->printf("Puppet Scenario: %u\n", puppetInfo->scenarioNo);
            gTextWriter->printf("Puppet Costume: H: %s B: %s\n", puppetInfo->costumeHead, puppetInfo->costumeBody);

            if (puppetInfo->isCaptured) {
                gTextWriter->printf("Current Capture: %s\n", puppetInfo->curHack);
                gTextWriter->printf("Current Packet Animation: %s\n", puppetInfo->curAnimStr);
                gTextWriter->printf("Animation Index: %d\n", puppetInfo->curAnim);
            } else {
                gTextWriter->printf("Current Packet Animation: %s\n", puppetInfo->curAnimStr);
                gTextWriter->printf("Animation Index: %d\n", puppetInfo->curAnim);

                al::LiveActor* curModel = puppet->getCurrentModel();
                if (curModel) {
                    gTextWriter->printf("Current Animation: %s\n", al::getActionName(curModel));
                }
            }
            break;
        }
    case 1:
        {
            PlayerHackKeeper* hackKeeper = playerBase->getPlayerHackKeeper();

            if (!hackKeeper)
                break;

            if(hackKeeper->currentHackActor) {

                al::LiveActor *curHack = hackKeeper->currentHackActor;

                gTextWriter->printf("Current Hack Animation: %s\n", al::getActionName(curHack));

                const char* hackName = hackKeeper->getCurrentHackName();
                gTextWriter->printf("Current Hack Name: %s\n", hackName);
                sead::Quatf captureRot = curHack->mPoseKeeper->getQuat();
                gTextWriter->printf("Current Hack Rot: %f %f %f %f\n", captureRot.x,
                                    captureRot.y, captureRot.z, captureRot.w);
                sead::Quatf calcRot;
                al::calcQuat(&calcRot, curHack);
                gTextWriter->printf("Calc Hack Rot: %f %f %f %f\n", calcRot.x,
                                    calcRot.y, calcRot.z, calcRot.w);
            } else { 
                PlayerActorHakoniwa *player = (PlayerActorHakoniwa*)playerBase; // its safe to assume that we're using a playeractorhakoniwa if the hack keeper isnt null
                
                gTextWriter->printf("Cur Action: %s\n", player->mPlayerAnimator->mAnimFrameCtrl->getActionName());
                gTextWriter->printf("Cur Sub Action: %s\n", player->mPlayerAnimator->curSubAnim.cstr());
                gTextWriter->printf("Is Cappy Flying? %s\n", BTOC(player->mHackCap->isFlying()));
                if(player->mHackCap->isFlying()) {
                    gTextWriter->printf("Cappy Action: %s\n", al::getActionName(player->mHackCap));
                    sead::Vector3f *capTrans = al::getTransPtr(player->mHackCap);
                    sead::Vector3f *capRot = &player->mHackCap->mJointKeeper->mJointRot;
                    gTextWriter->printf("Cap Coords:\nX: %f\nY: %f\nZ: %f\n", capTrans->x, capTrans->y, capTrans->z);
                    gTextWriter->printf("Cap Rot:\nX: %f\nY: %f\nZ: %f\n", capRot->x, capRot->y, capRot->z);
                    gTextWriter->printf("Cap Skew: %f\n", player->mHackCap->mJointKeeper->mSkew);
                }
            }
            break;
        }
    }

    gTextWriter->endDraw();
    return true;
}

void DebugMenu::drawBackground() {
    sead::Vector3<float> tl(-1, .3, 0); // top left
    sead::Vector3<float> tr(-.2, .3, 0); // top right
    sead::Vector3<float> bl(-1, -1, 0); // bottom left
    sead::Vector3<float> br(-.2, -1, 0); // bottom right
    sead::Color4f color(.1, .1, .1, .9);

    agl::DrawContext *context = ImmOverlayMenu::sInstance->context;
    agl::utl::DevTools::beginDrawImm(context,
                                     sead::Matrix34<float>::ident,
                                     sead::Matrix44<float>::ident);
    agl::utl::DevTools::drawTriangleImm(context, tl, tr, bl, color);
    agl::utl::DevTools::drawTriangleImm(context, bl, br, tr, color);
}

