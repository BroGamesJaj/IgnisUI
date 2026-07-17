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

	int dead = Audio::Open("../Resources/Audios/already dead.wav");

	int hearth = Audio::OpenStream("../Resources/Audios/Timber Hearth.wav");

	int dnd = Audio::OpenStream("../Resources/Audios/i drink and drive.wav");

	Audio::Play(dnd);
	Audio::Skip(dnd, 119.1f);
	Audio::Speed(dnd, 1.1f);
	Audio::Volume(dnd, 1.3f);

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
		else if (input == "resume") {
			std::cin >> input;
			Audio::Resume(std::stoi(input));
		}
		else if (input == "skip") {
			std::cin >> input;
			std::string input2;
			std::cin >> input2;
			Audio::Skip(std::stoi(input), std::stof(input2));
		}
		else if (input == "vol"){
			std::cin >> input;
			std::string input2;
			std::cin >> input2;
			Audio::Volume(std::stoi(input), std::stof(input2));
		}
		else if (input == "speed") {
			std::cin >> input;
			std::string input2;
			std::cin >> input2;
			Audio::Speed(std::stoi(input), std::stof(input2));
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
