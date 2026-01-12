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
#include <optional>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct GLFWwindow;
struct GLFWmonitor;

//tmp definitions, so its not all grayed out
#define IGNIS_UI
#define IGNIS_NAMES

namespace Ignis {

#if defined(IGNIS_RENDER) || defined(IGNIS_UI)
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
            CreateRenderPassInfo(){};

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
        class Vulkan;

        Vulkan* instance;

        friend class UI;

        int AddUIElementData(UIRenderData& data);
    };
#endif

#ifdef IGNIS_UI
    class UI {
    public:
        struct Vec2 {
            Vec2() : x(0), y(0) {}
            Vec2(float x, float y) : x(x), y(y) {}

            float x;
            float y;

            Vec2 operator+(Vec2 other) {
                this->x += other.x;
                this->y += other.y;
                return *this;
            }
        };

        struct Color {
            Color(unsigned char r, unsigned char g, unsigned char b)
                : r(r), g(g), b(b) {}

            Color() = default;

            unsigned char r;
            unsigned char g;
            unsigned char b;
        };

    private:
        enum UIType {
            TEXT,
            BUTTON,
            IMAGE,
            VIEW
        };

        struct UIData{
            UIData(void* ptr, UIType type);

            void* ptr;
            UIType type;
        };

        struct ElementData {
            Vec2 position;
            Vec2 size;
            int textureId;
            Color color;
        };

        struct TextData {
            ElementData base;
            std::string text;
        };

        struct ImageData {
            ElementData base;
        };

        class Element;

        struct ViewData {
            ElementData base;
            std::vector<Element> elements;
        };

        class Element {
        protected:
            UIData* data;
        public:
            Element(UIType type) : data(CreateData(type)),
                position(GetPosition(data)), size(GetSize(data)), id(nextId++), textureId(GetTexture(data)), color(GetColor(data)) {
                if (dataPtrs.contains(data))
                    dataPtrs[data]++;
                else
                    dataPtrs[data] = 1;
            }

            Vec2& position;
            Vec2& size;

            ~Element() {
                if (!data) return;

                dataPtrs[data]--;

                if (dataPtrs[data] <= 0) {
                    dataPtrs.erase(data);
                    DeleteData(data);
                }
            }

            bool Valid() { return data; }

        protected:
            const int id;
            int& textureId;
            Color& color;

            friend class UI;
        };

    public:
        class Text : public Element {
        public:
            Text() : Element(TEXT), text(GetText(data)) {}

            Text(Vec2 position, Vec2 size, std::string text) 
                : Element(TEXT), text(GetText(data)) {
                this->position = position;
                this->size = size;
                this->text = text;
                this->textureId = -1;
                this->color = Color{ (unsigned char)255, (unsigned char)255, (unsigned char)255 };
            }

            std::string& text;

            friend class UI;
        };

    private:
        struct ButtonData {
            ElementData base;
            void* function;
            Text text;
        };

    public:
        class Image : public Element {
        public:
            Image(Vec2 position, Vec2 size, std::optional<int> textureId = std::nullopt,
                std::optional<Color> color = std::nullopt)
                : Element(IMAGE) {

                if (textureId.has_value())
                    this->textureId = textureId.value();
                else
                    this->textureId = -1;

                if (color.has_value())
                    this->color = color.value();
                else
                    this->color = Color{ (unsigned char)255, (unsigned char)255, (unsigned char)255 };

                if (!textureId.has_value() && !color.has_value())
                    throw std::runtime_error("No visual data has been set for the image");

                this->position = position;
                this->size = size;
            }

            friend class UI;
        };

        class View : public Element {
            View(Vec2 position, Vec2 size, std::optional<int> textureId, std::optional<Color> color)
                : Element(VIEW), elements(GetChildrens(data)) {
                if (textureId.has_value())
                    this->textureId = textureId.value();
                else
                    this->textureId = -1;

                if (color.has_value())
                    this->color = color.value();
                else
                    this->color = Color{ (unsigned char)255, (unsigned char)255, (unsigned char)255 };
            }

            std::vector<Element>* elements;

            void Add(Element& element) {
                elements->push_back(element);
            }

            void Pop(Element& element) {
            }

            friend class UI;
        };

        class Button : public Element {
            Button(Vec2 position, Vec2 size, void* function, std::optional<Text>& text, std::optional<int> textureId, std::optional<Color> color)
                : Element(BUTTON), function(GetFunction(data)), text(GetTextElement(data)) {
                if (text.has_value())
                    Bind(this->text, text.value());

                this->position = position;
                this->size = size;

                if (textureId.has_value())
                    this->textureId = textureId.value();
                else
                    this->textureId = -1;

                if (color.has_value())
                    this->color = color.value();
                else
                    this->color = Color{ (unsigned char)255, (unsigned char)255, (unsigned char)255 };
            }

            Text& text;
            void*& function;

            friend class UI;
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
                        0, 2, 1, 0, 3, 2
                },
                .surface = surface,
                .changed = true,
            };

            return renderInstance->AddUIElementData(data);
        }

        static void AddToSurface(Element element, int surface = mainSurface);

        static void SubmitSurface(int surface = mainSurface);

        static void Bind(Element& dst, Element& src);

    private:
        static Render* renderInstance;
        static int mainSurface;
        static std::unordered_map<int, std::vector<Element>> elements;
        static int nextId;
        static std::unordered_map<UIData*, int> dataPtrs;

        struct ProcessData {
            Vec2 ofst;
            Vec2 size;
        };

        struct UIVertexData {
            std::vector<Render::Vertex> vertecies;
            std::vector<uint32_t> indicies;
        };

        static Render::UIRenderData ProcessVertecies(ProcessData data, std::vector<Element>& elements);
        static UIVertexData GenerateVertecies(UI::ProcessData procData, int textureId, Color color);

        //Data Handling
        static UIData* CreateData(UIType type);
        static void DeleteData(UIData* data);

        //Element
        static Vec2& GetPosition(UIData* data);
        static Vec2& GetSize(UIData* data);

        //Text
        static std::string& GetText(UIData* data);

        //Image
        static int& GetTexture(UIData* data);
        static Color& GetColor(UIData* data);

        //View
        static std::vector<Element>* GetChildrens(UIData* data);

        //Button
        static void*& GetFunction(UIData* data);
        static Text& GetTextElement(UIData* data);
    };
#endif
}