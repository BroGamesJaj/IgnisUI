#define IGNIS_UI
#define IGNIS_UI_NAMES
#define IGNIS_RENDER_NAMES
#include "../../IgnisLib/Source/IgnisLib.h"
#include "../../IgnisLib/Source/Font.h"

using namespace Ignis;

//Resize callback
void Resized(Window window) {
	Vec2i curSize = Input::WindowSize(window);
	std::cout << "Curent window size: " << curSize.x << "; " << curSize.y << std::endl;
}

int main() {
    std::vector<int> surfaces;

	//need to initialize the input before using
	Input::Init();

	CreateGraphicPipeLineInfo gpInfo{};
	gpInfo.vertexShader = "../Resources/Shaders/shader.vert";
	gpInfo.fragmentShader = "../Resources/Shaders/shader.frag";

	Render::Init(true);

	Window window1 = Render::CreateAppWindow(1200, 800, "Gup 1");

	//need to initialize the window & hook a callback witch has a window as an input
	Input::InitWindow(window1);
	Input::HookFramebufferSizeCallback(window1, Resized);

	int surface = Render::CreateSurface(window1, gpInfo);
	surfaces.push_back(surface);

	int monika = Render::CreateTexture("../Resources/Textures/monika2.png");
	int sus = Render::CreateTexture("../Resources/Textures/goated0.bmp");

	int fontId = UI::LoadFont("../DejaVuSans.ttf");

	UI::SetMainSurface(surface);

	Color tip(0, 255, 0);
	Color base = tip.Inverted();

	//Text text = Text(Vec2f(40, 10), Vec2f(100,20), "heooo fak yeah", fontId);

	Image image = Image( Vec2f(40, 10), Vec2f(20, 40), monika);
	Image image3 = Image(Vec2f(45, 15), Vec2f(20, 40), sus);
	Image image2 = Image( Vec2f(25, 40), Vec2f(50, 20), tip);
	UI::AddToSurface(surface, image2, image, image3);
	UI::SubmitSurface();

	while (surfaces.size() != 0)
	{
		Input::Event();
		Render::Update();

		for (auto it = surfaces.begin(); it != surfaces.end(); ) {
			if (Render::IsValidSurface(*it)) {
				Render::Draw(*it);
				it++;
			}
			else it = surfaces.erase(it);
		}
	}

    UI::Clean();
}
