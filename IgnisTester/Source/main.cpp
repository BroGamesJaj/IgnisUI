#define IGNIS_UI
#define IGNIS_UI_NAMES
#define IGNIS_RENDER_NAMES
#include "../../IgnisLib/Source/IgnisLib.h"

using namespace Ignis;

// need to initialize the input before using
// => Input::Init();
// => Input::InitWindow(window1); maytodo: this init should be enough, we doesnt need the basic one, like with the ui
// Resize callback
// => Input::HookInputCursorPositionCallback(window1, CursorMoved);
// called in a for loop to update inputs
// => Input::Event();
void CursorMoved(Window window) {
    Vec2<double> curPos = Input::CursorPosition(window);
    std::cout << "Curent cursor position: " << curPos.x << "; " << curPos.y << std::endl;
}

int main() {
    Input::Init();
    Render::Init(true);
    Window window1 = Render::CreateAppWindow(1200, 800, "Gup 1");
    // Window window2 = Render::CreateAppWindow(1200, 800, "Gup 1");

    /*
    // int fontId = UI::LoadFont("../DejaVuSans.ttf");
    */
    UI::Init(window1);
    UI::SetMainWindow(window1);
    Texture monika = UI::CreateTexture("../Resources/Textures/monika2.png");
    Texture sus = UI::CreateTexture("../Resources/Textures/goated0.bmp");

    // Image image = Image(Vec2f(50, 20), Vec2f(10, 10), Color(0.0f, 1.0f, 1.0f));
    std::vector<UI::Image> images = {
        Image(Vec2f(20, 70), Vec2f(40, 55), monika),
        Image(Vec2f(50, 40), Vec2f(30, 40), monika),
        Image(Vec2f(80, 70), Vec2f(40, 55), monika)
    };
    // should add popoff so elements can be removed seperately

    UI::PushOn(images);
    // UI::PushOn(window2, images);
    UI::Submit();
    // UI::Submit(window2);

    float t = 0.0f;

    while (Render::IsOpen()) {
        t += 0.0001f;

        UI::Rotate(images[1], ((1 % 2 == 0) ? 1 : -1) * -0.05f);
        UI::Move(images[1], { cos(t + 1) * 0.008f, sin(t + 0 / 2) * 0.008f });
        UI::ColorChange(images[1], t * 100 + 1 * 0.2f);

        Input::Event();
        Render::Update();
    }

    UI::CleanUp();
    Render::CleanUp();
    Input::CleanUp();
}
