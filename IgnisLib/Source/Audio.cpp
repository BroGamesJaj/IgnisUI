#include "IgnisLib.h"

#include <filesystem>
#include <thread>


namespace Ignis {

	void Audio::Test(std::string path) {
		std::filesystem::path cwd = std::filesystem::current_path();
		std::cout << "Current working directory: " << cwd << std::endl;

		std::cout << path << std::endl;
		/*
		while (!ma_sound_at_end(sound)) {
			//no thing
		}
		*/
	}
}