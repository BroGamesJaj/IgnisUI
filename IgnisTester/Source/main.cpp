#define IGNIS_UI
#define IGNIS_UI_NAMES
#define IGNIS_RENDER_NAMES
#include "../../IgnisLib/Source/IgnisLib.h"
#include "../../IgnisLib/Source/Font.h"

using namespace Ignis;

//Resize callback
void CursorMoved(Window window) {
	Vec2<double> curPos = Input::CursorPosition(window);
	std::cout << "Curent cursor position: " << curPos.x << "; " << curPos.y << std::endl;
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
	//Input::HookInputCursorPositionCallback(window1, CursorMoved);

	int surface = Render::CreateSurface(window1, gpInfo);
	surfaces.push_back(surface);

	//int monika = Render::CreateTexture("../Resources/Textures/monika2.png");
	//int sus = Render::CreateTexture("../Resources/Textures/goated0.bmp");

	int fontId = UI::LoadFont("../DejaVuSans.ttf");

	UI::SetMainSurface(surface);

	Color tip(0, 255, 0);
	Color base = tip.Inverted();

	Text text = Text(Vec2f(40, 10), Vec2f(100,100), "heooo fak yeah", fontId);

	View view = View(Vec2f(40, 10), Vec2f(20, 20), base);
	Image image2 = Image(Vec2f(25, 40), Vec2f(50, 20), tip);
	view.Add(image2);
	view.Add(text);
	UI::AddToSurface(surface, view);
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
