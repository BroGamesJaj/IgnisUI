#include "../IgnisLib.h"

namespace Ignis {

std::vector<char> FileSystem::ReadFile(const std::string &filename) {
    // std::ios::ate - start reading from end
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    // because we read from the end of the file, the current position is the size of the needed buffer
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    // reads the whole file in
    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();
    return buffer;
}

void FileSystem::CompileShader(const std::string filename, const std::string name) {
    std::string shaderOut = name + ".spv";

#if defined(_WIN32)
    const char *envVar = "VULKAN_SDK";
    char *value = nullptr;
    size_t len = 0;
    if (_dupenv_s(&value, &len, envVar) == 0 && value) {
        std::string sdkPath = value;
        free(value);

        std::string cmd = sdkPath + "\\Bin\\glslc.exe " + filename + " -o " + shaderOut;
        system(cmd.c_str());
    }
#else
    const char *sdkPath = std::getenv("VULKAN_SDK");
    std::string cmd;
    if (sdkPath) {
        cmd = std::string(sdkPath) + "/bin/glslc " + filename + " -o " + shaderOut;
    } else {
        cmd = "glslc " + filename + " -o " + shaderOut;  // fallback if VULKAN_SDK not set
    }
    system(cmd.c_str());
#endif
}

}  // namespace Ignis
