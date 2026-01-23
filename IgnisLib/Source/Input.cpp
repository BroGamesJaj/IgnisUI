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


void Input::HookWindowPosCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, WindowPos}] = function;
}
void Input::HookWindowSizeCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, WindowSize}] = function;
}
void Input::HookWindowcloseCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, Windowclose}] = function;
}
void Input::HookWindowRefreshCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, WindowRefresh}] = function;
}
void Input::HookWindowFocusCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, WindowFocus}] = function;
}
void Input::HookWindowIconifyCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, WindowIconify}] = function;
}
void Input::HookWindowMaximizeCallback(Render::Window window, HookFunction function) {
	customCallbacks[{window.ptr, WindowMaximize}] = function;
}
void Input::HookFramebufferSizeCallback(Render::Window window, HookFunction function) {
	customCallbacks[{window.ptr, FramebufferSize}] = function;
}

void Input::HookInputKeyCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, InputKey}] = function;
}
void Input::HookInputCharCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, InputChar}] = function;
}
void Input::HookInputCharModsCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, InputCharMods}] = function;
}
void Input::HookInputMouseButtonCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, InputMouseButton}] = function;
}
void Input::HookInputCursorEnterCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, InputCursorEnter}] = function;
}
void Input::HookInputScrollCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, InputScroll}] = function;
}
void Input::HookInputDropCallback(Render::Window window, HookFunction function) {
	customCallbacks[{window.ptr, InputDrop}] = function;
}



void Input::Init() {
	glfwInit();
}

void Input::Event() {
	glfwPollEvents();
}

std::unordered_map<std::pair<GLFWwindow*, int>, Input::HookFunction, Ignis::Input::KeyHash> Ignis::Input::customCallbacks;

Vec2i windowPosition;
Vec2i windowSize;
Vec2i frameBuffersize;

Input::Keydata Input::Key;
unsigned int Input::Char;
Vec2<double> Input::cursorPosition;
bool Input::cursorEntered;
Vec2<double> Input::scrollOffset;
Input::DropData Input::dropElements;

};