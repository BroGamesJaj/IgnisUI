#define GLFW_INCLUDE_VULKAN
#include "glfw3.h"

#include "../../IgnisLib/Source/IgnisLib.h"

using namespace Ignis;

int main() {
	std::vector<int> surfaces;

	glfwInit();

	Ignis::Render::CreateGraphicPipeLineInfo gpInfo{};
	gpInfo.vertexShader = "../Resources/Shaders/shader.vert";
	gpInfo.fragmentShader = "../Resources/Shaders/shader.frag";

	Ignis::Render renderer = Ignis::Render(true);

	Ignis::Render::Window window1 = renderer.CreateAppWindow(500, 400, "Gup 1");
	int surface = renderer.CreateSurface(window1, gpInfo);
	surfaces.push_back(surface);

	renderer.CreateTexture("../Resources/Textures/monikaTexture.jpg");

	Ignis::UI::SetRender(&renderer);
	Ignis::UI::SetMainSurface(surface);

	UI::Color tip(255,0,255);
	UI::Color base(255,0,0);

	Image image = Image( UI::Vec2( 20,30 ), UI::Vec2( 20, 40 ), std::nullopt, base);
	Image image2 = Image(UI::Vec2(25, 40), UI::Vec2(50, 20), std::nullopt, tip);
	UI::AddToSurface(image);
	UI::AddToSurface(image2);
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
}
