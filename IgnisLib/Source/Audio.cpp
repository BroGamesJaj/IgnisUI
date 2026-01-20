#include "IgnisLib.h"

#include "miniaudio/miniaudio.h"
#include <filesystem>
#include <thread>


namespace Ignis {

	void on_sound_end(void* pUserData, ma_sound* pSound) {
		// your script or code here
		printf("Sound finished!\n");
		ma_sound_uninit(pSound);
		delete pSound;
	}

	void Audio::Test(std::string path) {
		std::filesystem::path cwd = std::filesystem::current_path();
		std::cout << "Current working directory: " << cwd << std::endl;

		std::cout << path << std::endl;

		ma_engine* engine = new ma_engine();
		ma_engine_init(NULL, engine);

		ma_decoder* decoder = new ma_decoder();
		ma_decoder_init_file(path.c_str(), NULL, decoder);

		ma_sound* sound = new ma_sound();
		ma_sound_init_from_data_source(engine, decoder, 0, NULL, sound);

		ma_sound_set_volume(sound, 1.0f);

		ma_sound_start(sound);

		ma_sound_set_end_callback(sound, on_sound_end, NULL);
		/*
		while (!ma_sound_at_end(sound)) {
			//no thing
		}
		*/
	}
}