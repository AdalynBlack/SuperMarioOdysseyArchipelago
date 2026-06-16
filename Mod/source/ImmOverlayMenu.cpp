#include "ImmOverlayMenu.h"

ImmOverlayMenu *ImmOverlayMenu::sInstance = nullptr;

void ImmOverlayMenu::init(sead::Heap *heap, agl::DrawContext *context) {
    if (!context)
        return;

    if (sInstance)
        if(sInstance->context)
            return;

    sInstance = new ImmOverlayMenu(heap, context);
}

void ImmOverlayMenu::draw(sead::Viewport *viewport, HakoniwaSequence *HakoniwaSequence, bool isInGame) {
    if (!sInstance)
        return;

    sInstance->drawOverlayMenu(viewport, HakoniwaSequence, isInGame);
}

void ImmOverlayMenu::drawOverlayMenu(sead::Viewport *viewport, HakoniwaSequence *sequence, bool isInGame) {
    gTextWriter->mViewport = viewport;
    gTextWriter->mColor = sead::Color4f(1.f, 1.f, 1.f, 0.8f);

    int dispHeight = al::getLayoutDisplayHeight();

    if (!debug::DebugMenu::draw(viewport, sequence, isInGame, dispHeight))
        drawChat(viewport, sequence, isInGame, dispHeight);
}

void ImmOverlayMenu::drawChat(sead::Viewport *viewport, HakoniwaSequence *sequence, bool isInGame, int dispHeight) {
    static int messageShiftTimer = 0;

    if (Client::getQueueLength() == 0)
        messageShiftTimer = 0;

    // This equation should probably be chagned to a lerp eventually
    if (messageShiftTimer > (5 - Client::getQueueLength()) * 150)
    {
        messageShiftTimer = 0;
        Client::shiftMessages();
    }

    if (Client::getQueueLength() <= 0)
        return;

    drawBackground(Client::getQueueLength());

    gTextWriter->beginDraw();
    gTextWriter->setScaleFromFontHeight(15.f);

    int height_offset = (6 - Client::getQueueLength());
    for (int i = 0; i < Client::getQueueLength(); i++)
    {
        gTextWriter->setCursorFromTopLeft(sead::Vector2f(10.f, (dispHeight * 7 / 10) + 18.f * (i + height_offset)));
        printf_colored(Client::getAPChatMessage(i).cstr());
    }

    messageShiftTimer++;
    gTextWriter->endDraw();
}

void ImmOverlayMenu::drawBackground(int lines) {
    sead::Vector3<float> tl(-1, -.7 + 0.05 * lines, 0);    // top left
    sead::Vector3<float> tr(-.2, -.7 + 0.05 * lines, 0);  // top right
    sead::Vector3<float> bl(-1, -.7, 0);   // bottom left
    sead::Vector3<float> br(-.2, -.7, 0);  // bottom right
    sead::Color4f color(.1, .1, .1, .7);

    agl::utl::DevTools::beginDrawImm(context,
                                     sead::Matrix34<float>::ident,
                                     sead::Matrix44<float>::ident);
    agl::utl::DevTools::drawTriangleImm(context, tl, tr, bl, color);
    agl::utl::DevTools::drawTriangleImm(context, bl, br, tr, color);
}


void ImmOverlayMenu::printf_colored(const char *message) {
    int length = strlen(message);
    for (int i = 0; i < length; i++)
    {
        // Ansi color code starts here
        if(message[i] == '\x1b') {
            // Skip to the number. We don't care about the rest
            i += 2;

            // Ansi code was incomplete
            if (i >= length)
                return;

            int color_code = atoi(message + i);

            // Scan until we find the termination character
            do {
                i++;
                // Ansi code was incomplete
                if (i >= length)
                    return;
            } while (message[i] != 'm');

            // Ansi code was incomplete
            if (i >= length)
                return;

            switch (color_code)
            {
                // No BG colors, bold, or underline support. Just ignore them
                case 0: // Reset
                case 37: // White
                    gTextWriter->mColor = sead::Color4f(1.f, 1.f, 1.f, .8f);
                    break;
                case 30: // Black
                    gTextWriter->mColor = sead::Color4f(0.f, 0.f, 0.f, .8f);
                    break;
                case 31: // Red
                    gTextWriter->mColor = sead::Color4f(1.f, 0.f, 0.f, .8f);
                    break;
                case 32: // Green
                    gTextWriter->mColor = sead::Color4f(0.f, 1.f, 0.5f, .8f);
                    break;
                case 33: // Yellow
                    gTextWriter->mColor = sead::Color4f(1.f, 1.f, 0.467f, .8f);
                    break;
                case 34: // Blue
                    gTextWriter->mColor = sead::Color4f(0.392f, 0.584f, 0.929f, .8f);
                    break;
                case 35: // Magenta
                    gTextWriter->mColor = sead::Color4f(1.f, 0.f, 1.f, .8f);
                    break;
                case 36: // Cyan
                    gTextWriter->mColor = sead::Color4f(0.f, 1.f, 1.f, .8f);
                    break;
            }
        } else {
            gTextWriter->printf("%c", message[i]);
        }
    }
}

ImmOverlayMenu::ImmOverlayMenu(sead::Heap *heap, agl::DrawContext *context) {
    if (!al::isExistFile(DBG_SHADER_PATH))
        return;
    if (!al::isExistFile(DBG_FONT_PATH))
        return;
    if (!al::isExistFile(DBG_TBL_PATH))
        return;

    sead::DebugFontMgrJis1Nvn::sInstance = sead::DebugFontMgrJis1Nvn::createInstance(heap);
    sead::DebugFontMgrJis1Nvn::sInstance->initialize(heap, DBG_SHADER_PATH, DBG_FONT_PATH, DBG_TBL_PATH, 0x100000);
    sead::TextWriter::setDefaultFont(sead::DebugFontMgrJis1Nvn::sInstance);
    

    this->context = context;
    this->gTextWriter = new sead::TextWriter(context);
    this->gTextWriter->setupGraphics(context);
}
