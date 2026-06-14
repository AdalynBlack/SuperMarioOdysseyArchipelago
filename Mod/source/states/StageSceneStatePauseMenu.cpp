#include "game/StageScene/StageSceneStatePauseMenu.h"

void StageSceneStatePauseMenu::exeServerConfig(void) {
    if (al::isFirstStep(this)) {
        Logger::log("Start Server Config Nerve.\n");
    }

    al::updateKitListPrev(mHost);
    rs::requestGraphicsPresetAndCubeMapPause(mHost);
    al::updateKitList(mHost, "２Ｄ（ポーズ無視）");
    al::updateKitListPost(mHost);

    if (al::updateNerveState(this)) {
        if (mStateOption->isChangeLanguage() || mStateOption->field_0x180) {
            kill();
        } else {
            mSelectParts->appearWait();

            mFooterParts->tryChangeTextFade(
                al::getSystemMessageString(mMenuGuide, "Footer", "MenuMessage_Footer"));

            if (al::isNerve(this, &nrvStageSceneStatePauseMenuServerConfig)) {
                al::setNerve(this, &nrvStageSceneStatePauseMenuStartHelp);
            } else {
                al::setNerve(this, &nrvStageSceneStatePauseMenuWait);
            }
        }
    }
}
