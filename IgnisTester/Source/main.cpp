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

    /*
    // int fontId = UI::LoadFont("../DejaVuSans.ttf");
    */
    UI::Init(window1);
    UI::SetMainWindow(window1);
    Texture monika = UI::CreateTexture("../Resources/Textures/monika2.png");
    Texture sus = UI::CreateTexture("../Resources/Textures/goated0.bmp");

    Image image = Image(Vec2f(45, 45), Vec2f(10, 10), Color(0.5f, 1.0f, 1.0f));
    Image image3 = Image(Vec2f(40, 35), Vec2f(20, 30), monika);
    // should add popoff so elements can be removed seperately
    UI::PushOn(image3, image);
    UI::Submit();

    while (Render::IsOpen()) {
        Input::Event();
        Render::Update();  // i need to draw
    }

    UI::CleanUp();
    Render::CleanUp();
    Input::CleanUp();
}
