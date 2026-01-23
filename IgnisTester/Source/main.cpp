#define GLFW_INCLUDE_VULKAN
#include "glfw3.h"

#define IGNIS_UI
#define IGNIS_UI_NAMES
#define IGNIS_RENDER_NAMES
#include "../../IgnisLib/Source/IgnisLib.h"
#include "../../IgnisLib/Source/Font.h"

using namespace Ignis;

int main() {
    std::vector<int> surfaces;

    glfwInit();

    CreateGraphicPipeLineInfo gpInfo{};
    gpInfo.vertexShader = "../Resources/Shaders/shader.vert";
    gpInfo.fragmentShader = "../Resources/Shaders/shader.frag";

    Render renderer = Render(true);

    Window window1 = renderer.CreateAppWindow(500, 400, "Gup 1");
    int surface = renderer.CreateSurface(window1, gpInfo);
    surfaces.push_back(surface);

    int monika1 = renderer.CreateTexture("../Resources/Textures/monikaTexture.jpg");
    //int monika2 = renderer.CreateTexture("../Resources/Textures/monika3d.jpeg");

    UI::SetRender(&renderer);
    UI::SetMainSurface(surface);

    int fontId = UI::LoadFont("../DejaVuSans.ttf");

    Color tip(255, 255, 255);
    Color base(255, 0, 0);

    Image image = Image(Vec2(0, 0), Vec2(50, 50),monika1,base);
    Image image2 = Image(Vec2(25, 25), Vec2(50, 50),2,tip);
    //Image image2 = Image(Vec2(25, 40), Vec2(50, 20), tip);
    Text text = Text(Vec2(0, 0), Vec2(10, 10),"j o e died.:(!",fontId);
    Text text2 = Text(Vec2(0, 10), Vec2(10, 10),"gyalog"/*gjoedied:(!"*/,fontId);
    Text text3 = Text(Vec2(0, 20), Vec2(10, 10),"gas"/*gjoedied:(!"*/,fontId);
    UI::AddToSurface(surface, image2, image);
    UI::AddToSurface(surface, text,text2);
    UI::SubmitSurface();

    while (surfaces.size() != 0) {
        renderer.Event();

        for (auto it = surfaces.begin(); it != surfaces.end();) {
            if (renderer.IsValidSurface(*it)) {
                renderer.Draw(*it);
                it++;
            } else
                it = surfaces.erase(it);
        }
    }

    UI::Clean();
}
