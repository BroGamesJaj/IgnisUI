#include "IgnisLib.h"

#include "rtAudio/RtAudio.h"

#include <thread>
#include <fstream>
#include <cstdint>
#include <vector>
#include <string>
#include <cstring>

#define BUFFER_SIZE 4096
#define CACHE_SIZE BUFFER_SIZE*8

namespace Ignis {

    struct PlayingAudio {
        uint32_t instanceId;
        uint32_t id;
    };

    struct WavData {
        uint32_t sampleRate;
        uint16_t channels;
        uint16_t bitsPerSample;

        uint32_t dataSize;
        std::streampos dataOffset;
        uint32_t readOffset = 0;
		uint32_t cacheOffset = 0;

        std::vector<char> cache;
        std::vector<char> buffer;
        std::unordered_map<int, uint32_t> instanceOffset;
        std::ifstream file;
        float volume = 1.0f;
        float speed = 1.0f;

    private:
        uint32_t leftOnCache = CACHE_SIZE;
        bool cacheEnd = false;
    public:
        bool bufferEnd = false;
        bool haveData = false;
        bool loop = false;
        bool isStream = false;

        void ReadFile(const std::string& path) {
            cache.resize(CACHE_SIZE);
            buffer.resize(BUFFER_SIZE);

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
                    file.read(reinterpret_cast<char*>(&audioFormat), sizeof(audioFormat));
                    file.read(reinterpret_cast<char*>(&channels), sizeof(channels));
                    file.read(reinterpret_cast<char*>(&sampleRate), sizeof(sampleRate));

                    file.ignore(6);

                    file.read(reinterpret_cast<char*>(&bitsPerSample), sizeof(bitsPerSample));

                    file.ignore(chunkSize - 16);
                }
                else if (id == "data") {
                    if (bitsPerSample % 8 != 0) {
                        std::cerr << "Unsupported bits per sample: " << bitsPerSample << std::endl;
                        return;
                    }

                    dataSize = chunkSize;
                    dataOffset = file.tellg();

                    haveData = true;
                    return;
                }
                else {
                    file.ignore(chunkSize);
                }
            }
        }

        void Open(const std::string& path) {
            ReadFile(path);

            if (haveData) {
                cache.resize(dataSize);
                file.clear();
                file.read(cache.data(), dataSize);

                leftOnCache = dataSize;
                readOffset = dataSize;
            }

            file.close();
        }

        void OpenStream(const std::string& path) {
			isStream = true;

            ReadFile(path);

            if (haveData) {
                uint32_t C = CACHE_SIZE;
                if (dataSize < CACHE_SIZE)
                    C = dataSize;

                file.clear();
                file.read(cache.data(), C);
                readOffset = C;
            }
        }
    private:
        void ReadToCache() {
            uint32_t leftOver = CACHE_SIZE - cacheOffset;
			memmove(cache.data(), cache.data() + cacheOffset, leftOver);

            uint32_t leftOnFile = dataSize - readOffset;
            if (leftOnFile == 0 && !loop) return;

			uint32_t newSize = CACHE_SIZE - leftOver;

            if (leftOnFile < newSize) {
                file.read(cache.data() + leftOver, leftOnFile);
				readOffset += leftOnFile;
                if (loop) {
                    file.clear();
                    file.seekg(dataOffset);
                    file.read(cache.data() + leftOnFile + leftOver, newSize - leftOnFile);
					readOffset = newSize - leftOnFile;
                }
                else {
                    cacheEnd = true;
					leftOnCache = leftOver + leftOnFile;
                    cacheOffset = 0;
                }

                return;
            }

            file.read(cache.data() + leftOver, newSize);

			readOffset += newSize;

			cacheOffset = 0;

            return;
        }

    public:

        uint32_t ReadOff(uint32_t size, int index = -1) {
            if (size > BUFFER_SIZE) {
                std::cerr << "Requested size exceeds buffer size" << std::endl;
                return 0;
            }

            if (isStream) {
                if (!cacheEnd && cacheOffset + size > 0.9 * CACHE_SIZE) {
                    ReadToCache();
                }

                if (cacheEnd && cacheOffset + size > leftOnCache) {
                    bufferEnd = true;
                    size = leftOnCache - cacheOffset;
                }

                memcpy(buffer.data(), cache.data() + cacheOffset, size);
                cacheOffset += size;
                return size;
            }
            else {
                if (index == -1) throw std::runtime_error("didn't give instance index to instanced audio readoff");

                if (size + instanceOffset[index] > leftOnCache) {
                    if (loop) {
                        uint32_t leftOver = leftOnCache - instanceOffset[index];
                        memcpy(buffer.data(), cache.data() + cacheOffset, leftOver);
                        leftOver = size - leftOver;
                        memcpy(buffer.data() + leftOver, cache.data(), leftOver);
                        instanceOffset[index] = leftOver;

                        return size;
                    }
                    else {
                        bufferEnd = true;
                        size = leftOnCache - instanceOffset[index];
                    }
                }

                memcpy(buffer.data(), cache.data() + instanceOffset[index], size);
                instanceOffset[index] += size;
                return size;
            }
        }

        friend class Audio;
    };

    std::vector<WavData> data;
    std::vector<PlayingAudio> playing;
    RtAudio dac;

    RtAudio::DeviceInfo def;

    int outputDevice = -1;

    uint32_t instanceCount = 1;


    void LoadedPath(std::string& path, int index) {
        size_t lastSlash = path.find_last_of("/\\");
        size_t lastDot = path.find_last_of('.');

        std::string name = path.substr(lastSlash + 1, lastDot - lastSlash - 1);

        std::cout << "Loaded " << name << " into index " << index << std::endl;
    }

    void CacheReset(WavData& dat) {
        dat.readOffset = 0;
        dat.cacheOffset = 0;
        dat.file.seekg(dat.dataOffset);
        dat.cache.clear();
        dat.cache.resize(CACHE_SIZE);
    }

    int music(void* outputBuffer, void* inputBuffer,unsigned int nBufferFrames,double streamTime, RtAudioStreamStatus status, void* userData)
    {
        int16_t* out = (int16_t*)outputBuffer;

        if (status)
            std::cout << "underflow\n";

        uint32_t frames = nBufferFrames * def.outputChannels;

        for (size_t i = 0; i < frames; i++)
        {
            out[i] = 0;
        }

        for (auto it = playing.begin(); it != playing.end();) {
            PlayingAudio audio = *it;

            WavData& curData = data[audio.id];

            float step = (float)curData.sampleRate * curData.speed / 44100;

            uint32_t neededFrames = (uint32_t)std::ceil(nBufferFrames * step) + 1; // +1 for interpolation lookahead
            uint32_t neededBytes = neededFrames * def.outputChannels * sizeof(int16_t);

            size_t idIndex = it - playing.begin();

            uint32_t got;
            if (!curData.isStream) got = curData.ReadOff(neededBytes, audio.instanceId);
            else got = curData.ReadOff(neededBytes);


            int16_t* src = reinterpret_cast<int16_t*>(curData.buffer.data());

            if (curData.bufferEnd) {
                it = playing.erase(it);
                curData.instanceOffset.erase(idIndex);
                curData.bufferEnd = false;
            }
            else
                ++it;

            int channels = def.outputChannels;

            float pos = 0.0f;

            uint32_t frame;
            int base;
            int base2;

            for (uint32_t i = 0; i < nBufferFrames; i++) {

                frame = (int)pos;
                if (frame + 1 >= (int)got) break;
                float frac = pos - frame;

                base = frame * channels;
                base2 = (frame + 1) * channels;

                for (int c = 0; c < channels; c++) {

                    int16_t s1 = src[base + c];
                    int16_t s2 = src[base2 + c];

                    int16_t sample = (int16_t)(s1 + (s2 - s1) * frac);

                    int32_t mixed = (int32_t)out[i * channels + c] + (int32_t)sample;
                    mixed *= curData.volume;
                    mixed = std::clamp(mixed, (int32_t)INT16_MIN, (int32_t)INT16_MAX);
                    out[i * channels + c] = (int16_t)mixed;
                }

                pos += step;
            }
        }

        return 0;
    }

    int Audio::Open(std::string path) {
        WavData wavData;
        wavData.loop = true;
        wavData.Open(path);

        int index = data.size();

        data.push_back(std::move(wavData));

        LoadedPath(path, index);

        return index;
    }

	int Audio::OpenStream(std::string path) {

		WavData wavData;
		wavData.loop = true;
		wavData.OpenStream(path);

        int index = data.size();

        data.push_back(std::move(wavData));

        LoadedPath(path, index);

        return index;
	}

    void Audio::Play(int id) {

        if (!data[id].isStream) {
            data[id].instanceOffset[instanceCount] = 0;
        }
        else {
            bool exists = std::any_of(playing.begin(), playing.end(),
                [id](const PlayingAudio& a) {
                    return a.id == id;
                });
            if (exists) return;

            CacheReset(data[id]);
            data[id].ReadToCache();
        }

        PlayingAudio audio{ instanceCount++, id };

        playing.push_back(audio);
    }

    void Audio::Pause(int id) {
        for (auto it = playing.begin(); it != playing.end();) {
            uint32_t index = (*it).id;

            if (index == id) {
                it = playing.erase(it);
                if(data[id].isStream) return;
            }
            else ++it;
        }
    }

    void Audio::Resume(int id) {
        PlayingAudio audio;
        if (!data[id].isStream) {
            for (auto& [instance, offset] : data[id].instanceOffset) {

                //O(n) so not very happy about it, if shit ass performance, should enhance
                bool exists = std::any_of(playing.begin(), playing.end(),
                    [instance](const PlayingAudio& a) {
                        return a.instanceId == instance;
                    });

                if (exists) continue;

                audio.instanceId = instance;
                audio.id = id;
                playing.push_back(audio);
            }
        }
        else {
            audio.instanceId = instanceCount++;
            audio.id = id;
            playing.push_back(audio);
        }
    }

    void Audio::Stop(int id) {
        for (auto it = playing.begin(); it != playing.end();) {
            uint32_t index = (*it).id;

            if (index == id) {
                WavData& dat = data[id];
                if (dat.isStream) {
                    it = playing.erase(it);
                    CacheReset(dat);
                    dat.ReadToCache();
                    return;
                }
                else {
                    it = playing.erase(it);
                    dat.instanceOffset.clear();
                }
            }
            else ++it;
        }
    }

    void Audio::Skip(int id, float sec) {
        for (auto it = playing.begin(); it != playing.end();) {
            uint32_t index = (*it).id;

            if (index == id) {
                WavData& dat = data[id];
                if (!dat.isStream) return;
                dat.readOffset += (uint32_t)((float)dat.sampleRate * dat.speed * sec) * dat.channels * sizeof(int16_t);
                dat.readOffset = dat.readOffset % dat.dataSize;
                dat.file.seekg(dat.dataOffset + static_cast<std::streamoff>(dat.readOffset));
                dat.cacheOffset = 0;
                dat.cache.clear();
                dat.cache.resize(CACHE_SIZE);
                dat.ReadToCache();
                return;
            }
            else ++it;
        }
    }

    void Audio::Volume(int id, float volume) {
        WavData& dat = data[id];
        dat.volume = volume;
    }

    void Audio::Speed(int id, float speed) {
        WavData& dat = data[id];
        dat.speed = speed;
    }
    
    std::vector<Audio::Device> Audio::GetDevices() {
        std::vector<Device> devices;

        std::vector<unsigned int> ids = dac.getDeviceIds();

        RtAudio::DeviceInfo info;
        for (unsigned int n = 0; n < ids.size(); n++) {

            info = dac.getDeviceInfo(ids[n]);

            Device dev;
            dev.id = ids[n];
            dev.name = info.name;

            devices.push_back(std::move(dev));
        }

        return devices;
    }

    void Audio::SetOutputDevice(int id) {
        outputDevice = id;
    }

    void Audio::On() {
        std::vector<unsigned int> ids = dac.getDeviceIds();
        if (ids.size() < 1) {
            std::cout << "\nNo audio devices found!\n";
            exit(0);
        }

        RtAudio::DeviceInfo info;
        unsigned int sampleRate;
        for (unsigned int n = 0; n < ids.size(); n++) {

            info = dac.getDeviceInfo(ids[n]);

            if (info.isDefaultOutput == false) continue;
            std::cout << "device name = " << info.name << std::endl;
            std::cout << "duplex channels = " << info.duplexChannels << std::endl;

            //need a selector here
            for (size_t i = 0; i < info.sampleRates.size(); i++)
            {
                if (info.sampleRates[i] == 44100) {
                    sampleRate = info.sampleRates[i];
                }
            }
            def = info;
        }

        RtAudio::StreamParameters parameters;
        if (outputDevice >= 0)
            parameters.deviceId = outputDevice;
        else
            parameters.deviceId = dac.getDefaultOutputDevice();
        parameters.nChannels = 2;
        parameters.firstChannel = 0;
        unsigned int bufferFrames = 128;

        if (dac.openStream(&parameters, NULL, RTAUDIO_SINT16, sampleRate,
            &bufferFrames, &music, nullptr)) {
            std::cout << '\n' << dac.getErrorText() << '\n' << std::endl;
            exit(0);
        }

        if (dac.startStream()) {
            std::cout << dac.getErrorText() << std::endl;
            Off();
        }
    }

    void Audio::Off() {
        for (auto& item : data) {
            item.file.close();
        }

        // Block released ... stop the stream
        if (dac.isStreamRunning())
            dac.stopStream();  // or could call dac.abortStream();

        if (dac.isStreamOpen()) dac.closeStream();
    }
}