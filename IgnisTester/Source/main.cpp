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

struct UniformData {
	int x;
	int y;
};

int main() {
    std::vector<int> surfaces;

	//need to initialize the input before using/
	Input::Init();
	Render::Init(true);

	CreateGraphicPipeLineInfo gpInfo{};
	gpInfo.vertexShader = "../Resources/Shaders/shader.vert";
	gpInfo.fragmentShader = "../Resources/Shaders/shader.frag";
	gpInfo.blendEnable = true;
    gpInfo.descriptorSets = {  
		{
		   {
				Render::CreateUniformDescriptor(0, sizeof(UniformData), Render::ShaderStage::VERTEX),
				Render::CreateImageDescriptor(1, 1028, Render::CreateSampler(), Render::ShaderStage::FRAGMENT),
		   },
		   0
		}
    };
	gpInfo.constantsSize = sizeof(float);

	Window window1 = Render::CreateAppWindow(1200, 800, "Gup 1");

	//need to initialize the window & hook a callback witch has a window as an input
	Input::InitWindow(window1);
	//Input::HookInputCursorPositionCallback(window1, CursorMoved);

	int surface = Render::CreateSurface(window1, gpInfo);
	surfaces.push_back(surface);

	int monika = Render::CreateTexture("../Resources/Textures/monika2.png");
	int sus = Render::CreateTexture("../Resources/Textures/goated0.bmp");

	//int fontId = UI::LoadFont("../DejaVuSans.ttf");

	UI::SetMainSurface(surface);

	Color tip(0, 255, 0);
	Color base = tip.Inverted();

	//Text text = Text(Vec2f(40, 10), Vec2f(100,100), "heooo fak yeah", fontId);

	View view = View(Vec2f(40, 10), Vec2f(20, 20), base);
	Image image2 = Image(Vec2f(25, 40), Vec2f(50, 20), tip);
	Image image3 = Image(Vec2f(0, 0), Vec2f(100, 100), monika);
	view.Add(image2);
	UI::AddToSurface(surface, image3, view);
	UI::SubmitSurface();

	float color = 0.0;

	while (surfaces.size() != 0)
	{
		Input::Event();
		Render::Update();

		if (color > 1.0f) color = 0.0f;

		color += 0.004f;

		for (auto it = surfaces.begin(); it != surfaces.end(); ) {
			if (Render::IsValidSurface(*it)) {
				Render::PushConstants(*it, &color, sizeof(color));
				Render::Draw(*it);
				it++;
			}
			else it = surfaces.erase(it);
		}
	}

    UI::Clean();
}
