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

	Window window1 = renderer.CreateAppWindow(500, 400, "Gup 1");
	int surface = renderer.CreateSurface(window1, gpInfo);
	surfaces.push_back(surface);

	renderer.CreateTexture("../Resources/Textures/monikaTexture.jpg");

	UI::SetRender(&renderer);
	UI::SetMainSurface(surface);

	Color tip(255,0,255);
	Color base(255,0,0);

	Image image = Image( Vec2(20, 30), Vec2(20, 40), base);
	Image image2 = Image( Vec2(25, 40), Vec2(50, 20), tip);
	UI::AddToSurface(surface, image, image2);
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
