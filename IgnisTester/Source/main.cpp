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

struct Test {
  int a;
};

int main() {
    Input::Init();
    Render::Init(true);

    Window window1 = Render::CreateAppWindow(1200, 800, "Gup 1");

    //int fontId = UI::LoadFont("../DejaVuSans.ttf");
    UI::Init(window1);
    UI::SetMainWindow(window1);

    Texture monika = UI::CreateTexture("../Resources/Textures/monika2.png");
    Texture sus = UI::CreateTexture("../Resources/Textures/goated0.bmp");
    

    Image image = Image(Vec2f(0,0), Vec2f(100,100), Color(1.0f,1.0f,1.0f));
    Image image3 = Image(Vec2f(0, 0), Vec2f(100, 100), monika);
    UI::PushOn(window1, image);
    UI::Submit();
    
    float time = 0.0f;

    while (UI::CanDraw()) {
        Input::Event();
        Render::Update();

        UI::Draw();
    }

    UI::Clean();
}
