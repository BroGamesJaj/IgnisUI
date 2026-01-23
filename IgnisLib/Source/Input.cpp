#include "IgnisLib.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

namespace Ignis {

//All the glfw callbacks
//Handle: the data got from the callback is managed by the Input system
//Hook: the user needs to hook a function to get "pinged" when it is set of and to get the data
enum Callbacks {
	//Window
	WindowPos,				//Handle
	WindowSize,				//Handle
	Windowclose,			//Hook
	WindowRefresh,			//Hook
	WindowFocus,			//Hook
	WindowIconify,			//Hook
	WindowMaximize,			//Hook
	FramebufferSize,		//Handle
	WindowContentScale,		//Not implemented
	//Input
	InputKey,				//Handle
	InputChar,				//Handle
	InputCharMods,			//Handle
	InputMouseButton,		//Handle
	InputCursorEnter,		//Hook
	InputScroll,			//Handle
	InputDrop,				//Hook
	//Monitor
	Monitor,				//Not implemented
	//Error
	Error					//Not implemented
};

void Input::HookFramebufferSizeCallback(Render::Window window, HookFunction function) {
	customCallbacks[{window.ptr, FramebufferSize}] = function;
}


void Input::Init() {
	glfwInit();
}

void Input::Event() {
	glfwPollEvents();
}

std::unordered_map<std::pair<GLFWwindow*, int>,void*,Ignis::Input::KeyHash> Ignis::Input::customCallbacks;
};