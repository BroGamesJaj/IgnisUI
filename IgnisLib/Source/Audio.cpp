#include "IgnisLib.h"

#include "rtAudio/RtAudio.h"

#include <thread>
#include <fstream>
#include <cstdint>
#include <vector>
#include <string>

#define BUFFER_SIZE 4096
#define CACHE_SIZE BUFFER_SIZE*8

namespace Ignis {

    struct WavData {
        int sampleRate;
        int channels;
        int bitsPerSample;

        uint32_t dataSize;
        std::streampos dataOffset;
        uint32_t readOffset = 0;
		uint32_t cacheOffset = 0;

        char cache[CACHE_SIZE];
        char buffer[BUFFER_SIZE];
        std::ifstream file;

    private:
        uint32_t leftOnCache = CACHE_SIZE;
        bool cacheEnd = false;
    public:
        bool bufferEnd = false;
        bool haveData = false;
        bool loop = false;

        void OpenStream(const std::string& path) {
            file = std::ifstream(path, std::ios::binary);
            char riff[4];
            file.read(riff, 4); // "RIFF"
            file.ignore(4);     // file size
            char wave[4];
            file.read(wave, 4); // "WAVE"

            while (file) {
                char chunkId[4];
                if (!file.read(chunkId, 4)) break;

                uint32_t chunkSize;
                file.read(reinterpret_cast<char*>(&chunkSize), 4);

                std::string id(chunkId, 4);

                if (id == "fmt ") {
                    uint16_t audioFormat;
                    file.read(reinterpret_cast<char*>(&audioFormat), 2);
                    file.read(reinterpret_cast<char*>(&channels), 2);
                    file.read(reinterpret_cast<char*>(&sampleRate), 4);

                    file.ignore(6); // byte rate + block align
                    file.read(reinterpret_cast<char*>(&bitsPerSample), 2);

                    file.ignore(chunkSize - 16);
                }
                else if (id == "data") {
                    if(bitsPerSample % 8 != 0) {
                        std::cerr << "Unsupported bits per sample: " << bitsPerSample << std::endl;
                        return;
					}

                    dataSize = chunkSize;
                    dataOffset = file.tellg();

					uint32_t C = CACHE_SIZE;
                    if (dataSize < CACHE_SIZE)
                        C = dataSize;

                    file.clear();
                    file.read(cache, C);
                    readOffset = CACHE_SIZE;

                    haveData = true;
                    return;
                }
                else {
                    file.ignore(chunkSize);
                }
            }
        }
    private:
        void ReadToCache() {
            uint32_t leftOver = CACHE_SIZE - cacheOffset;
			memmove(cache, cache + cacheOffset, leftOver);

            uint32_t leftOnFile = dataSize - readOffset;
            if (leftOnFile == 0 && !loop) return;

			uint32_t newSize = CACHE_SIZE - leftOver;

            if (leftOnFile < newSize) {
                file.read(cache + leftOver, leftOnFile);
				readOffset += leftOnFile;
                if (loop) {
                    file.clear();
                    file.seekg(dataOffset);
                    file.read(cache + leftOnFile + leftOver, newSize - leftOnFile);
					readOffset = newSize - leftOnFile;
                }
                else {
                    cacheEnd = true;
					leftOnCache = leftOver + leftOnFile;
                }

                return;
            }

            file.read(cache + leftOver, newSize);

			readOffset += newSize;

			cacheOffset = 0;

            return;
        }

    public:

        uint32_t ReadOff(uint32_t size) {

            if(size > BUFFER_SIZE) {
                std::cerr << "Requested size exceeds buffer size" << std::endl;
                return;
			}

            if(!cacheEnd && cacheOffset + size > 0.9 * CACHE_SIZE) {
				ReadToCache();
			}

            if(cacheEnd && cacheOffset + size > leftOnCache) {
                bufferEnd = true;
                size = leftOnCache - cacheOffset;
			}
			
			memcpy(buffer, cache + cacheOffset, size);
            cacheOffset += size;
            return size;
        }
    };

    int music(void* outputBuffer, void* inputBuffer,unsigned int nBufferFrames,double streamTime, RtAudioStreamStatus status, void* userData)
    {
        int16_t* out = (int16_t*)outputBuffer;
        WavData* audio = (WavData*)userData;

        if (status)
            std::cout << "underflow\n";

        uint32_t frames = nBufferFrames * 2; // stereo

        uint32_t got = audio->ReadOff(frames * sizeof(int16_t));

        int16_t* src = (int16_t*)audio->buffer;

        for (uint32_t i = 0; i < frames; i++) {
            if (i < got / 2)
                out[i] = src[i];
            else
                out[i] = 0; // silence if underrun
        }

        return 0;
    }

	void Audio::Test(std::string path) {

		WavData wavData;
		wavData.OpenStream(path);

        RtAudio dac;
        std::vector<unsigned int> deviceIds = dac.getDeviceIds();
        if (deviceIds.size() < 1) {
            std::cout << "\nNo audio devices found!\n";
            exit(0);
        }

        RtAudio::StreamParameters parameters;
        parameters.deviceId = dac.getDefaultOutputDevice();
        parameters.nChannels = 2;
        parameters.firstChannel = 0;
        unsigned int sampleRate = 44100;
        unsigned int bufferFrames = 256; // 256 sample frames
        double data[2] = { 0, 0 };

        if (dac.openStream(&parameters, NULL, RTAUDIO_SINT16, sampleRate,
            &bufferFrames, &music, (void*)&wavData)) {
            std::cout << '\n' << dac.getErrorText() << '\n' << std::endl;
            exit(0); // problem with device settings
        }

        if (dac.startStream()) {
            std::cout << dac.getErrorText() << std::endl;
            goto cleanup;
        }

        char input;
        std::cout << "\nPlaying ... press <enter> to quit.\n";
        std::cin.get(input);

        // Block released ... stop the stream
        if (dac.isStreamRunning())
            dac.stopStream();  // or could call dac.abortStream();

    cleanup:
        if (dac.isStreamOpen()) dac.closeStream();
	}
}