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
#include <unordered_set>
#include <concepts>
#include <type_traits>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct GLFWwindow;
struct GLFWmonitor;

//tmp definitions, so its not all grayed out
#define IGNIS_UI
#define IGNIS_UI_NAMES
#define IGNIS_RENDER_NAMES
#define IGNIS_INPUT

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

        static void Init(bool debugging);
        static void Clean();
    
        static Window CreateAppWindow(int width, int height, const char* title, GLFWmonitor* screen = nullptr, GLFWwindow* share = nullptr);
        static int CreateSurface(Window window, CreateGraphicPipeLineInfo graphicPipeLineInfo, CreateRenderPassInfo renderPassInfo = {});
        static int CreateTexture(std::string path);

        static void Draw(int surface);
        static void Event();
        static bool IsValidSurface(int surfaceIndex);
    
    private:
        class Vulkan;

        static Vulkan* instance;

        friend class UI;

        static int AddUIElementData(UIRenderData& data);
    };

#ifdef IGNIS_RENDER_NAMES
    using Window = Render::Window;
    using CreateGraphicPipeLineInfo = Render::CreateGraphicPipeLineInfo;
#endif

#endif

#ifdef IGNIS_UI
    class UI {
    public:
        template <typename T>
            requires std::is_arithmetic_v<T>
        struct Vec2 {
            Vec2() : x(0), y(0) {}
            Vec2(T x, T y) : x(x), y(y) {}

            T x;
            T y;

            Vec2 operator+(const Vec2& other) const { return Vec2{x + other.x, y + other.y}; }
            Vec2 operator-(const Vec2& other) const { return Vec2{x - other.x, y - other.y}; }
            Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
            Vec2& operator-=(const Vec2& other) { x -= other.x; y -= other.y; return *this; }
            Vec2 operator*(const Vec2& other) const { return Vec2{x * other.x, y * other.y}; }
            Vec2 operator/(const Vec2& other) const { return Vec2{x / other.x, y / other.y}; }
            Vec2& operator*=(const Vec2& other) { x *= other.x; y *= other.y; return *this; }
            Vec2& operator/=(const Vec2& other) { x /= other.x; y /= other.y; return *this; }     
        };

        using Vec2f = Vec2<float>;
        using Vec2i = Vec2<int>;

        template <typename T>
            requires std::is_arithmetic_v<T>
        struct Area2 {
            Vec2<T> TL;
            Vec2<T> BR;

            Vec2<T> TR;
            Vec2<T> BL;

            inline void Calc();
            inline bool Contain(Vec2<T>& position) const;
        };

        struct Color {
            Color(int r, int g, int b)
                : r((float)r/255), g((float)g/255), b((float)b/255) {}

            Color(float r, float g, float b)
                : r(r), g(g), b(b) {}

            Color() : r(1), g(1), b(1) {}

            Color(std::string hex);

            float r;
            float g;
            float b;

            Color operator+(const Color& other) const;
            Color operator-(const Color& other) const;
            Color Inverted();
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
            Vec2i position;
            Vec2i size;
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
                if (!dataPtrs.contains(data))
                    dataPtrs.insert(data);
            }

            Vec2i& position;
            Vec2i& size;

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

            Text(Vec2i position, Vec2i size, std::string text) 
                : Element(TEXT), text(GetText(data)) {
                this->position = position;
                this->size = size;
                this->text = text;
                this->textureId = -1;
                this->color = Color();
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
        private:
            Image(Vec2i position, Vec2i size, int textureId, Color color, bool dummy)
                : Element(IMAGE) {
                this->position = position;
                this->size = size;

                this->textureId = textureId;
                this->color = color;
            }

        public:
            Image(Vec2i position, Vec2i size, int textureId, Color color)
                : Image(position, size, textureId, color, true) {}

            Image(Vec2i position, Vec2i size, Color color)
                : Image(position, size, -1, color, true) {}

            Image(Vec2i position, Vec2i size, int textureId)
                : Image(position, size, textureId, Color(), true) {}

            friend class UI;
        };

        class View : public Element {
            View(Vec2i position, Vec2i size, std::optional<int> textureId, std::optional<Color> color)
                : Element(VIEW), elements(GetChildrens(data)) {
                if (textureId.has_value())
                    this->textureId = textureId.value();
                else
                    this->textureId = -1;

                if (color.has_value())
                    this->color = color.value();
                else
                    this->color = Color();
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
            Button(Vec2i position, Vec2i size, void* function, std::optional<Text>& text, std::optional<int> textureId, std::optional<Color> color)
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
                    this->color = Color();
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

        template<std::derived_from<UI::Element>... Args>
        static void AddToSurface(int surface, Args&... args) {
            if (!renderInstance->IsValidSurface(surface)) return;
            (elements[surface].push_back(args), ...);
        }

        static void SubmitSurface(int surface = mainSurface);

        static void Bind(Element& dst, Element& src);

        static void Delete(Element& element);

        static void Clean();

    private:
        static Render* renderInstance;
        static int mainSurface;
        static std::unordered_map<int, std::vector<Element>> elements;
        static int nextId;
        static std::unordered_set<UIData*> dataPtrs;

        struct ProcessData {
            Vec2f ofst;
            Vec2f size;
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
        static Vec2i& GetPosition(UIData* data);
        static Vec2i& GetSize(UIData* data);

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

#ifdef IGNIS_UI_NAMES
    using Color = UI::Color;
    using Vec2i = UI::Vec2i;
    using Vec2f = UI::Vec2f;

    using Image = UI::Image;
    using Text = UI::Text;
    using View = UI::View;
    using Button = UI::Button;
#endif

#endif

#if defined(IGNIS_INPUT) || defined(IGNIS_UI)
    class Input {
        static void InitWindow();
    };
#endif
}