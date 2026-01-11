#define GLFW_INCLUDE_VULKAN
#include "glfw3.h"

#include "../../IgnisLib/Source/IgnisLib.h"

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
	Ignis::UI::CreateButton();

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
