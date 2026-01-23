#define GLFW_INCLUDE_VULKAN
#include "glfw3.h"

#define IGNIS_UI
#define IGNIS_UI_NAMES
#define IGNIS_RENDER_NAMES
#include "../../IgnisLib/Source/IgnisLib.h"

using namespace Ignis;

int main() {
	std::vector<int> surfaces;

	glfwInit();

	CreateGraphicPipeLineInfo gpInfo{};
	gpInfo.vertexShader = "../Resources/Shaders/shader.vert";
	gpInfo.fragmentShader = "../Resources/Shaders/shader.frag";

	Render renderer = Render(true);

	Window window1 = renderer.CreateAppWindow(1200, 800, "Gup 1");
	int surface = renderer.CreateSurface(window1, gpInfo);
	surfaces.push_back(surface);

	int monika = renderer.CreateTexture("../Resources/Textures/monika2.png");
	int sus = renderer.CreateTexture("../Resources/Textures/goated0.bmp");

	UI::SetRender(&renderer);
	UI::SetMainSurface(surface);

	Color tip(0, 255, 0);
	Color base = tip.Inverted();

	Image image = Image( Vec2i(40, 10), Vec2i(20, 40), monika, base);
	Image image3 = Image(Vec2i(45, 15), Vec2i(20, 40), sus);
	Image image2 = Image( Vec2i(25, 40), Vec2i(50, 20), tip);
	UI::AddToSurface(surface, image2, image, image3);
	UI::SubmitSurface();

	while (surfaces.size() != 0)
	{
		renderer.Event();

		for (auto it = surfaces.begin(); it != surfaces.end(); ) {
			if (renderer.IsValidSurface(*it)) {
				renderer.Draw(*it);
				it++;
			}
			else it = surfaces.erase(it);
		}
	}

	UI::Clean();
}
