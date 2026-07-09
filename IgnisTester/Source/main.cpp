#define GLFW_INCLUDE_VULKAN
#include "glfw3.h"

#define IGNIS_UI
#define IGNIS_UI_NAMES
#define IGNIS_RENDER_NAMES
#include "../../IgnisLib/Source/IgnisLib.h"

#include <thread>
#include <chrono>

using namespace Ignis;

int main() {
	int mainDev = -1;

	std::vector<Audio::Device> devices =  Audio::GetDevices();
	for (auto& dev : devices) {
		std::cout << dev.name << " with id: " << dev.id << std::endl;
		if (dev.name.find("Redmi") != std::string::npos && dev.name.find("Stereo") != std::string::npos) {
			mainDev = dev.id;
		}
	}

	std::cout << "Found at " << mainDev << std::endl;

	Audio::SetOutputDevice(mainDev);

	Audio::On();

	std::string file = "../Resources/Audios/already dead.wav";



	int dead = Audio::Open(file);

	int dead2 = Audio::OpenStream("../Resources/Audios/Timber Hearth.wav");

	while (true) {
		std::string input;
		std::cout << std::endl;
		std::cin >> input;

		if (input == "pause") {
			std::cin >> input;
			Audio::Pause(std::stoi(input));
		}
		else if (input == "play") {
			std::cin >> input;
			Audio::Play(std::stoi(input));
		}
		else if (input == "stop") {
			std::cin >> input;
			Audio::Stop(std::stoi(input));
		}
		else if (input == "skip") {
			std::cin >> input;
			Audio::Skip(dead2, std::stof(input));
		}
		else if (input == "vol"){
			std::cin >> input;
			Audio::Volume(dead2, std::stof(input));
		}
		else if (input == "exit") {
			break;
		}
		else {
			std::cerr << "unknown command: " << input << std::endl;
		}
	}

	Audio::Off();
}
