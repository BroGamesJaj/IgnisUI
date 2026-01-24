#include "IgnisLib.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

namespace Ignis {

//All the glfw callbacks
//Handle: the data got from the callback is managed by the Input system
//Hook: the user needs to hook a function to get "pinged" when it is set of and to get the data
enum class Input::Callbacks : int {
	//Window
	WindowPos,				//Handle
	WindowSize,			//Handle
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
	InputCursorPosition,	//Handle
	InputCursorEnter,		//Hook
	InputScroll,			//Handle
	InputDrop,				//Hook
	//Monitor
	Monitor,				//Not implemented
	//Error
	Error					//Not implemented
};


void Input::HookWindowPosCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, Callbacks::WindowPos}] = function;
}
void Input::HookWindowSizeCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, Callbacks::WindowSize}] = function;
}
void Input::HookWindowCloseCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, Callbacks::Windowclose}] = function;
}
void Input::HookWindowRefreshCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, Callbacks::WindowRefresh}] = function;
}
void Input::HookWindowFocusCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, Callbacks::WindowFocus}] = function;
}
void Input::HookWindowIconifyCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, Callbacks::WindowIconify}] = function;
}
void Input::HookWindowMaximizeCallback(Render::Window window, HookFunction function) {
	customCallbacks[{window.ptr, Callbacks::WindowMaximize}] = function;
}
void Input::HookFramebufferSizeCallback(Render::Window window, HookFunction function) {
	customCallbacks[{window.ptr, Callbacks::FramebufferSize}] = function;
}

void Input::HookInputKeyCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, Callbacks::InputKey}] = function;
}
void Input::HookInputCharCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, Callbacks::InputChar}] = function;
}
void Input::HookInputCharModsCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, Callbacks::InputCharMods}] = function;
}
void Input::HookInputMouseButtonCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, Callbacks::InputMouseButton}] = function;
}
void Input::HookInputCursorPositionCallback(Render::Window window, HookFunction function) {
	customCallbacks[{window.ptr, Callbacks::InputCursorPosition}] = function;
}
void Input::HookInputCursorEnterCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, Callbacks::InputCursorEnter}] = function;
}
void Input::HookInputScrollCallback(Render::Window window, HookFunction function){
	customCallbacks[{window.ptr, Callbacks::InputScroll}] = function;
}
void Input::HookInputDropCallback(Render::Window window, HookFunction function) {
	customCallbacks[{window.ptr, Callbacks::InputDrop}] = function;
}


void Input::CallFunction(GLFWwindow* window, Callbacks type) {
	auto it = customCallbacks.find({ window, type });
	if (it == customCallbacks.end() || it->second == nullptr)
		return;

	HookFunction functionTmp = reinterpret_cast<HookFunction>(customCallbacks[{window, type}]);
	Render::Window win;
	win.ptr = window;
	functionTmp(win);
}


void Input::WindowPosCallback(GLFWwindow* window, int xpos, int ypos){
	windowCallbackData[window].windowPosition = { xpos, ypos };
	CallFunction(window, Callbacks::WindowPos);
}
void Input::WindowSizeCallback(GLFWwindow* window, int width, int height){
	windowCallbackData[window].windowSize = { width, height };
	CallFunction(window, Callbacks::WindowSize);
}
void Input::WindowCloseCallback(GLFWwindow* window){
	CallFunction(window, Callbacks::Windowclose);
}
void Input::WindowRefreshCallback(GLFWwindow* window){
	CallFunction(window, Callbacks::WindowRefresh);
}
void Input::WindowFocusCallback(GLFWwindow* window, int focused){
	CallFunction(window, Callbacks::WindowFocus);
}
void Input::WindowIconifyCallback(GLFWwindow* window, int iconified){
	CallFunction(window, Callbacks::WindowIconify);
}
void Input::WindowMaximizeCallback(GLFWwindow* window, int maximized){
	CallFunction(window, Callbacks::WindowMaximize);
}
void Input::FramebufferSizeCallback(GLFWwindow* window, int width, int height){
	windowCallbackData[window].framebufferSize = { width, height };
	CallFunction(window, Callbacks::FramebufferSize);
}

void Input::InputKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods){
	windowCallbackData[window].Key = { key, scancode, action, mods };
	CallFunction(window, Callbacks::InputKey);
}
void Input::InputCharCallback(GLFWwindow* window, unsigned int codepoint){
	windowCallbackData[window].Char = codepoint;
	CallFunction(window, Callbacks::InputChar);
}
void Input::InputCharModsCallback(GLFWwindow* window, unsigned int codepoint, int mods){
	windowCallbackData[window].modChar = { codepoint, mods };
	CallFunction(window, Callbacks::InputCharMods);
}
void Input::InputMouseButtonCallback(GLFWwindow* window, int button, int action, int mods){
	windowCallbackData[window].Mouse = { button, action, mods };
	CallFunction(window, Callbacks::InputMouseButton);
}
void Input::InputCursorPositionCallback(GLFWwindow* window, double xpos, double ypos){
	windowCallbackData[window].cursorPosition = { xpos, ypos };
	CallFunction(window, Callbacks::InputCursorPosition);
}
void Input::InputCursorEnterCallback(GLFWwindow* window, int entered){
	windowCallbackData[window].Mouse.entered = entered;
	CallFunction(window, Callbacks::InputCursorEnter);
}
void Input::InputScrollCallback(GLFWwindow* window, double xoffset, double yoffset){
	windowCallbackData[window].scrollOffset = { xoffset, yoffset };
	CallFunction(window, Callbacks::InputScroll);
}
void Input::InputDropCallback(GLFWwindow* window, int count, const char** paths){
	windowCallbackData[window].Drop = { count, paths };
	CallFunction(window, Callbacks::InputDrop);
}


void ErrorLog(int error_code, const char* description) {
	std::cout << "Input error with glfw code: 1\nDescription: " << description << std::endl;
}


Vec2i Input::WindowPosition(Render::Window window){
	return windowCallbackData[window.ptr].windowPosition;
}
Vec2i Input::WindowSize(Render::Window window){
	return windowCallbackData[window.ptr].windowSize;
}
Vec2i Input::FramebufferSize(Render::Window window){
	return windowCallbackData[window.ptr].framebufferSize;
}
Input::Keydata Input::Key(Render::Window window){
	return windowCallbackData[window.ptr].Key;
}
unsigned int Input::Char(Render::Window window){
	return windowCallbackData[window.ptr].Char;
}
Input::ModifierData Input::ModChar(Render::Window window){
	return windowCallbackData[window.ptr].modChar;
}
Input::MouseData Input::Mouse(Render::Window window){
	return windowCallbackData[window.ptr].Mouse;
}
Vec2<double> Input::CursorPosition(Render::Window window){
	return windowCallbackData[window.ptr].cursorPosition;
}
Vec2<double> Input::Scroll(Render::Window window){
	return windowCallbackData[window.ptr].scrollOffset;
}
Input::DropData Input::Drop(Render::Window window) {
	return windowCallbackData[window.ptr].Drop;
}


void Input::Init() {
	glfwInit();
	glfwSetErrorCallback(ErrorLog);
}

void Input::Event() {
	glfwPollEvents();
}

void Input::InitWindow(Window& window) {
	GLFWwindow* ptr = window.ptr;

	WindowInputData data;

	glfwGetWindowPos(window.ptr, &data.windowPosition.x, &data.windowPosition.y);
	glfwGetWindowSize(window.ptr, &data.windowSize.x, &data.windowSize.y);
	glfwGetFramebufferSize(window.ptr, &data.framebufferSize.x, &data.framebufferSize.y);
	glfwGetCursorPos(window.ptr, &data.cursorPosition.x, &data.cursorPosition.y);

	glfwSetWindowPosCallback(ptr, WindowPosCallback);
	glfwSetWindowSizeCallback(ptr, WindowSizeCallback);
	glfwSetFramebufferSizeCallback(ptr, FramebufferSizeCallback);
	glfwSetWindowCloseCallback(ptr, WindowCloseCallback);
	glfwSetWindowRefreshCallback(ptr, WindowRefreshCallback);
	glfwSetWindowFocusCallback(ptr, WindowFocusCallback);
	glfwSetWindowIconifyCallback(ptr, WindowIconifyCallback);
	glfwSetWindowMaximizeCallback(ptr, WindowMaximizeCallback);

	glfwSetKeyCallback(ptr, InputKeyCallback);
	glfwSetCharCallback(ptr, InputCharCallback);
	glfwSetCharModsCallback(ptr, InputCharModsCallback);
	glfwSetMouseButtonCallback(ptr, InputMouseButtonCallback);
	glfwSetCursorPosCallback(ptr, InputCursorPositionCallback);
	glfwSetCursorEnterCallback(ptr, InputCursorEnterCallback);
	glfwSetScrollCallback(ptr, InputScrollCallback);
	glfwSetDropCallback(ptr, InputDropCallback);
}

std::unordered_map<std::pair<GLFWwindow*, Input::Callbacks>, Input::HookFunction, Input::KeyHash> Input::customCallbacks;
std::unordered_map <GLFWwindow*, Input::WindowInputData> Input::windowCallbackData;
};