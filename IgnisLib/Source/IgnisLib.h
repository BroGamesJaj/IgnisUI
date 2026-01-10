#pragma once

#include <iostream>
#include <vector>
#include <optional>
#include <set>
#include <limits>
#include <algorithm>
#include <fstream>
#include <array>
#include <string>
#include <unordered_map>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct GLFWwindow;
struct GLFWmonitor;

namespace Ignis {

    class Vulkan;

    class Render
    {
    public:
        //Rendering structs
        struct Vertex {
            glm::vec3 pos;
            glm::vec3 color;
            glm::vec2 texCoord;
            glm::uint texId;
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
            bool changed = true;
        };

        Render(bool debugging);
        ~Render();
    
        Window CreateAppWindow(int width, int height, const char* title, GLFWmonitor* screen = nullptr, GLFWwindow* share = nullptr);
        int CreateSurface(Window window, CreateGraphicPipeLineInfo graphicPipeLineInfo, CreateRenderPassInfo renderPassInfo = {});
        int CreateTexture(std::string path);

        void Draw(int surface);
        void Event();
        bool IsValidSurface(int surfaceIndex);
    
    private:
        Vulkan* instance;

        friend class UI;

        int AddUIElementData(UIRenderData& data);

        struct ProcessData {
            Vec2 offset;
            Vec2 size;

            std::vector<Vertex> vertecies;
            std::vector<uint32_t> indicies;
        };
    };

    class UI {
    public:
        struct Vec2 {
            float x;
            float y;
        };

        struct Color {
            char r;
            char g;
            char b;
        };

        struct Element {
            Vec2 position;
            Vec2 size;
            int id;

            virtual ~Element() = default;
        };

        struct Text : Element {
            std::string text;
        };

        struct Image : Element {
            int textureId;
            Color color;
        };

        struct Button : Element {
            Text text;
        };

        struct View : Element {
            std::vector<Element> elements;

            void Add(Element& element) {
                elements.push_back(element);
            }

            void Pop(Element& element) {
                auto it = std::find_if(elements.begin(), elements.end(),
                    [element](const Element& e) { return e.id == element.id;});
                if (it != elements.end()) {
                    elements.erase(it);
                }
            }
        };
        

        static inline void SetRender(Render* render) { renderInstance = render; }

        static inline void SetMainSurface(int surface) { mainSurface = surface; }

        static int CreateButton(int surface = mainSurface) {
            if (!renderInstance) {
                throw std::runtime_error("Render for UI has not been set");
            }

            if (!renderInstance->IsValidSurface(surface)) {
                throw std::runtime_error("Invalid surface for UI element");
            }

            Render::UIRenderData data{
                .vertecies = {
                    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0}},
                    {{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {0}},
                    {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0}},
                    {{-0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {0}}
                },
                .indicies = { 
                        0, 2, 1, 3, 2, 0
                },
                .surface = surface,
                .changed = true,
            };

            return renderInstance->AddUIElementData(data);
        }

        static void AddToSurface(Element& element, int surface = mainSurface);

        static void SubmitSurface(int surface = mainSurface);

    private:
        static Render* renderInstance;
        static int mainSurface;
        static std::unordered_map<int, std::vector<Element>> elements;

        static Render::ProcessData ProcessVertecies(Render::ProcessData data, std::vector<Element>& elements);
    };
}