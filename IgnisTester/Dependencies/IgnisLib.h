#pragma once

#include <iostream>
#include <vector>
#include <optional>
#include <set>
#include <limits>
#include <algorithm>
#include <fstream>
#include <array>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct GLFWwindow;
struct GLFWmonitor;

namespace Ignis {



    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 texCoord;
        //glm::uint texId; 
    };

    struct Window {
        GLFWwindow* ptr;
    };

    struct CreateRenderPassInfo {
        enum class Samples { x1, x2, x4, x8 };
        enum class LoadOp { Clear, Load, DontCare };
        enum class StoreOp { Store, DontCare };
        enum class ImageLayout { Undefined, PresentSrcKHR };

        Samples samples = Samples::x1;
        LoadOp loadOp = LoadOp::Clear;
        StoreOp storeOp = StoreOp::Store;
        LoadOp stencilLoadOp = LoadOp::DontCare;
        StoreOp stencilStoreOp = StoreOp::DontCare;
        ImageLayout initialLayout = ImageLayout::Undefined;
        ImageLayout finalLayout = ImageLayout::PresentSrcKHR;
    };

    struct CreateGraphicPipeLineInfo {
        std::string vertexShader;
        std::string fragmentShader;
        std::string geometryShader;
        
        //it should have so much else, like
        //multisampling, vertex setup, stuff like that
    };

    struct UIRenderData {
        std::vector<Vertex> vertecies;
        std::vector<uint32_t> indicies;
        int surface;
        bool changed;
    };


    class Vulkan;

    class Render
    {
    public:
        Render(bool debugging);
        ~Render();
    
        Window CreateAppWindow(int width, int height, const char* title, GLFWmonitor* screen = nullptr, GLFWwindow* share = nullptr);
        int CreateSurface(Window window, CreateGraphicPipeLineInfo graphicPipeLineInfo, CreateRenderPassInfo renderPassInfo = {});
        int CreateTexture();

        void Draw(int surface);
        void Event();
        bool IsValidSurface(int surfaceIndex);
    
    private:
        Vulkan* instance;

        friend class UI;

        int AddUIElementData(UIRenderData& data);
    };

    class UI {
    public:
        static inline void SetRender(Render* render) { renderInstance = render; }

        static inline void SetMainSurface(int surface) { mainSurface = surface; }

        static inline int CreateButton() { return CreateButton(mainSurface); }

        static int CreateButton(int surface) {
            if (!renderInstance) {
                throw std::runtime_error("Render for UI has not been set");
            }

            if (!renderInstance->IsValidSurface(surface)) {
                throw std::runtime_error("Invalid surface for UI element");
            }

            UIRenderData data;
            data.vertecies = {
                {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                {{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                {{-0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
            };
            data.indicies = {
                0, 2, 1, 3, 2, 0
            };

            data.surface = surface;
            data.changed = true;

            return renderInstance->AddUIElementData(data);
        }

    private:
        static Render* renderInstance;
        static int mainSurface;

    };
}

