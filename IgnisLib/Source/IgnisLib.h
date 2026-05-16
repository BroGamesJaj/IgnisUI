#pragma once

#include <algorithm>
#include <any>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct GLFWwindow;
struct GLFWmonitor;

// tmp definitions, so its not all grayed out
#define IGNIS_UI
#define IGNIS_UI_NAMES
#define IGNIS_RENDER_NAMES
#define IGNIS_INPUT

namespace Ignis {

namespace Font {
enum class TextDirection { LTR,
                           RTL,
                           BTT,
                           TTB,
                           GUESS };
enum class TextAlign { LEFT,
                       CENTER,
                       RIGHT,
                       GUESS };
enum class Script { LATIN,
                    CYRILLIC,
                    ARABIC,
                    DEVANAGARI,
                    THAI,
                    GREEK,
                    HANGUL,
                    HIRAGANA,
                    KATAKANA,
                    HAN,
                    TAMIL,
                    GUESS };
enum class Style { REGULAR,
                   BOLD,
                   ITALIC,
                   UNDERLINE };

class Font;
}  // namespace Font

template <typename T>
concept arithmetic = std::is_arithmetic_v<T>;

template <arithmetic T>
struct Vec2 {
    Vec2() : x(0), y(0) {}
    Vec2(T x, T y) : x(x), y(y) {}

    T x;
    T y;

    constexpr inline Vec2 operator+(const Vec2 &other) const noexcept { return Vec2{ x + other.x, y + other.y }; }
    constexpr inline Vec2 operator-(const Vec2 &other) const noexcept { return Vec2{ x - other.x, y - other.y }; }
    constexpr inline Vec2 &operator+=(const Vec2 &other) {
        x += other.x;
        y += other.y;
        return *this;
    }
    constexpr inline Vec2 &operator-=(const Vec2 &other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    constexpr inline Vec2 operator*(const Vec2 &other) const { return Vec2{ x * other.x, y * other.y }; }
    constexpr inline Vec2 operator*(const T &scalar) const { return { x * scalar, y * scalar }; }

    constexpr inline Vec2 operator/(const Vec2 &other) const { return Vec2{ x / other.x, y / other.y }; }
    constexpr inline Vec2 operator/(const T &scalar) const { return { x / scalar, y / scalar }; }

    constexpr inline Vec2 &operator*=(const Vec2 &other) {
        x *= other.x;
        y *= other.y;
        return *this;
    }
    constexpr inline Vec2 &operator/=(const Vec2 &other) {
        x /= other.x;
        y /= other.y;
        return *this;
    }
    constexpr inline Vec2 &operator*=(const T &scalar) {
        x *= scalar;
        y *= scalar;
        return *this;
    }
    constexpr inline Vec2 &operator/=(const T &scalar) {
        x /= scalar;
        y /= scalar;
        return *this;
    }

    constexpr inline bool operator==(const Vec2 &other) const noexcept { return x == other.x && y == other.y; }
    constexpr inline bool operator!=(const Vec2 &other) const noexcept { return x != other.x || y != other.y; }

    constexpr inline float distance(const Vec2 &other) const { return sqrt((other.x - x) * (other.x - x) + (other.y - y) * (other.y - y)); }
    constexpr inline float distanceCmp(const Vec2 &other) const {
        return (other.x - x) * (other.x - x) + (other.y - y) * (other.y - y);
    }

    constexpr inline Vec2 normalize() const { return Vec2(x, y) * isqrt(x * x + y * y); }
    constexpr inline float crossProduct(const Vec2 &other) const { return x * other.y - y * other.x; }
    constexpr inline float dotProduct(const Vec2 &other) const { return x * other.x + y * other.y; }
    constexpr inline const std::string toString() const { return "(" + std::to_string(x) + "," + std::to_string(y) + ")"; };

    // fast inverse sqrt
    static float isqrt(float number) {
        union {
            float f;
            uint32_t i;
        } conv;

        float x2;
        const float threehalfs = 1.5F;

        x2 = number * 0.5F;
        conv.f = number;
        conv.i = 0x5f3759df - (conv.i >> 1);
        conv.f = conv.f * (threehalfs - (x2 * conv.f * conv.f));
        return conv.f;
    }
};
using Vec2f = Vec2<float>;
using Vec2i = Vec2<int>;

#if defined(IGNIS_RENDER) || defined(IGNIS_UI)
class Render {
   public:
    // Rendering structs
    enum VertexDataType {
        FLOAT,
        UINT,
        VEC2,
        VEC3,
        VEC4
    };

    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 texCoord;
        glm::uint texId;
    };

    struct VertexData {
        std::vector<Render::Vertex> vertecies;
        std::vector<uint32_t> indicies;
    };

    struct GlyphInstance {
        glm::vec2 pos;
        glm::vec2 size;
        glm::vec4 uvRect;
        glm::uint pageId;
    };

    struct Window {
       private:
        GLFWwindow *ptr;

        friend class Vulkan;
        friend class Render;
        friend class Input;
        friend class UI;
    };

    struct Texture {
       private:
        int id;

        friend class Vulkan;
        friend class Render;
        friend class Input;
        friend class UI;
    };

    enum class SamplerFilter {
        NEAREST,
        LINEAR
    };
    enum class SamplerMipmapMode {
        NEAREST,
        LINEAR
    };

    enum class SamplerAddressing {
        REPEAT,
        MIRRORED_REPEAT,
        CLAMP_TO_EDGE,
        CLAMP_TO_BORDER,
        MIRROR_CLAMP_TO_EDGE,
    };

    enum ShaderStage : int {
        VERTEX = 1 << 0,
        FRAGMENT = 1 << 1,
        COMPUTE = 1 << 2
    };

    struct DescriptorInfo {
        DescriptorInfo() {};

        enum class DescriptorType { UNIFORM,
                                    STORAGE,
                                    IMAGE };

        DescriptorType type;
        int binding;
        int count;
        ShaderStage stage;
        int data;
    };

    struct DescriptorSetId {
        DescriptorSetId(uint32_t set, uint32_t idx) : set(set), idx(idx) {}
        uint32_t set;
        uint32_t idx;
    };

    struct DescriptorSetInfo {
        DescriptorSetInfo() {};

        DescriptorSetInfo(std::vector<DescriptorInfo> descriptorInfo, int setIndex)
            : descriptorInfo(std::move(descriptorInfo)), setIndex(setIndex) {}

        // uid must be a previously created DescriptorSetInfo uid
        // A DescriptorSetInfo should never try to reuse a DescriptorSetInfo that is also a reuse
        std::vector<DescriptorInfo> descriptorInfo;
        uint32_t setIndex;  // must be larger then the previous for now
    };

    struct ConstData {
        std::string name;
        uint32_t size;
    };

    struct CreateGraphicPipeLineInfo {
        CreateGraphicPipeLineInfo() {};

        enum class Samples { x1,
                             x2,
                             x4,
                             x8 };
        enum class Topology { Point,
                              Line,
                              Triangle };
        enum class Culling { Front,
                             Back,
                             None };
        enum class FrontFace { Clockwise,
                               CounterClockwise };
        enum class BlendFactor {
            SrcAlpha,
            DstAlpha,
            OneMinusSrcAlpha,
            OneMinusDstAlpha,
            SrcColor,
            DstColor,
            OneMinusSrcColor,
            OneMinusDstColor
        };
        enum class BlendMode { Add,
                               Sub,
                               Max,
                               Min };

        std::string vertexShader;
        std::string fragmentShader;

        float clearBit[3] = { 1.0f, 1.0f, 1.0f };
        float stencilBit[2] = { 1.0, 0.0f };

        Topology topology = Topology::Triangle;
        Culling culling = Culling::None;
        FrontFace frontFace = FrontFace::CounterClockwise;

        float lineWidth = 1.0f;

        Samples samples = Samples::x1;

        bool sampleShading = false;
        float minSampleShading = 1.0f;

        bool blendEnable = false;
        BlendFactor scrColorBlend = BlendFactor::SrcAlpha;
        BlendFactor dstColorBlend = BlendFactor::OneMinusSrcAlpha;
        BlendMode colorBlendOp = BlendMode::Add;
        BlendFactor scrAlphaBlend = BlendFactor::SrcAlpha;
        BlendFactor dstAlphaBlend = BlendFactor::OneMinusSrcAlpha;
        BlendMode alphaBlendOp = BlendMode::Add;

        bool depthTesting = false;
        bool depthWriting = false;

        std::vector<std::string> constants;
        std::vector<int> descriptorSetIds;

        std::vector<Render::VertexDataType> vertexDataLayout;
    };

    struct RenderData {
        std::vector<Vertex> vertecies;
        std::vector<uint32_t> indicies;

        int surface;
        bool changed = true;
    };

    struct Surface {
        Surface() = default;
        ~Surface() = default;
        int surface;

       private:
        friend class Render;
        friend class UI;
        friend class Input;
    };

    static void Init(bool debugging = false) {
        InitWindowManager(debugging);
        InitVulkanDataManager();
    }
    static void Clean();

    static void PushOn(VertexData &data, Surface &surface);
    static void Submit(Surface &surface);

    static Window CreateAppWindow(int width, int height, const char *title, GLFWmonitor *screen = nullptr, GLFWwindow *share = nullptr) {
        Window window{};
        window.ptr = CreateWindow(width, height, title, screen, share);
        CreateSurface(window.ptr);

        std::cout << "Window successfully created" << std::endl;

        return window;
    }
    static Texture CreateTexture(int descriptorId, std::string path);

    static int CreateDescriptorSet(DescriptorSetInfo &descriptorSetInfos);
    static std::vector<int> CreateDescriptorSet(std::vector<Render::DescriptorSetInfo> &descriptorSetInfo);
    static int CreatePipeline(CreateGraphicPipeLineInfo &gpInfo);
    static int CreateFontPage(const std::vector<uint8_t> &rgbaData, uint32_t width, uint32_t height);

    static void Draw(Surface surface);
    static void Clear(Window &window);
    static void Update();
    static bool IsValidSurface(Surface surface);

    static int CreateSampler(SamplerFilter filter = SamplerFilter::LINEAR,
                             SamplerAddressing addressing = SamplerAddressing::REPEAT, SamplerMipmapMode mipmapMode = SamplerMipmapMode::LINEAR);

    template <typename... Args>
    static void PublishConstants(std::vector<std::string> names) {
        std::vector<ConstData> result;
        int i = 0;
        (result.push_back({ names[i++], sizeof(Args) }), ...);
        PublishConstantsToVulkan(result);
    }

    template <typename T>
    static void PushConstant(std::string name, T data) {
        PushConstantsToVulkan(name, &data, sizeof(T));
    }

    static DescriptorInfo CreateUniformDescriptor(int binding, int size, int stage);
    static DescriptorInfo CreateStorageDescriptor(int binding, int size, int stage);
    static DescriptorInfo CreateImageDescriptor(int binding, int count, int sampler, int stage);

    static void PushVertexData(int surface, void *vertexData, uint32_t size);

    template <typename... Args>
        requires(std::same_as<Args, Render::VertexDataType> && ...)
    static std::vector<Render::VertexDataType> CreateVertexData(Args... args) {
        return { args... };
    }

   private:
    static std::unordered_map<int, VertexData> surfaceData;

    friend class UI;

    static void AddElementData(RenderData &data);

    static void *GetWindowOfSurface(int surface);

    static void PublishConstantsToVulkan(std::vector<ConstData> &data);
    static void PushConstantsToVulkan(std::string &name, void *data, uint32_t size);

    static GLFWwindow *CreateWindow(int width, int height, const char *title, GLFWmonitor *screen, GLFWwindow *share);
    static void CreateSurface(void *window);

    struct QueueFamilyIndices {
        struct QueueInfo {
            uint32_t family = UINT32_MAX;
            uint32_t index = UINT32_MAX;
            bool isFamilySet() { return family != UINT32_MAX; }
            bool isIndexSet() { return index != UINT32_MAX; }
        };

        QueueInfo graphics{};
        QueueInfo present{};
        QueueInfo compute{};
        QueueInfo transfer{};

        bool isComplete() { return graphics.isFamilySet() && present.isFamilySet() && compute.isFamilySet() && transfer.isFamilySet(); }
        bool isMinimumComplete() { return graphics.isFamilySet() && present.isFamilySet(); }
        bool isComputeUnique() { return graphics.family != compute.family || (graphics.family == compute.family && graphics.index != compute.index); }
        bool isTransferUnique() { return graphics.family != transfer.family || (graphics.family == transfer.family && graphics.index != transfer.index); }
    };
    static QueueFamilyIndices GetQueueFamilies();
    static void *QuerySwapChainSupport();
    static void *GetInstance();
    static void *GetDevice();
    static void *GetSurface();
    static void *GetPhyDevice();

    static void InitWindowManager(bool debugging);
    static void InitVulkanDataManager();

    class Vulkan;
    friend Vulkan;
    static Vulkan *instance;

    class WindowManager;
    friend WindowManager;
    static WindowManager *windowManager;

    class VulkanDataManager;
    friend VulkanDataManager;
    static VulkanDataManager *vulkanDataManager;
};

#ifdef IGNIS_RENDER_NAMES
using Window = Render::Window;
using CreateGraphicPipeLineInfo = Render::CreateGraphicPipeLineInfo;
using SamplerFilter = Render::SamplerFilter;
using SamplerAddressing = Render::SamplerAddressing;
using SamplerMipmapMode = Render::SamplerMipmapMode;
using Texture = Render::Texture;
using Vertex = Render::Vertex;
using VertexData = Render::VertexData;
using Surface = Render::Surface;
#endif

#endif

#ifdef IGNIS_UI

class UI {
   public:
    template <typename T>
        requires std::is_arithmetic_v<T>
    struct Area2 {
        Area2() = default;

        Area2(Vec2<T> x, Vec2<T> y) {
            TL = x;
            BR = y;
            Calc();
        }

        Vec2<T> TL;
        Vec2<T> BR;

        Vec2<T> TR;
        Vec2<T> BL;

        inline void Calc() {
            TR = Vec2<T>(BR.x, TL.y);
            BL = Vec2<T>(TL.x, BR.y);
        }
        inline bool Contains(Vec2<T> &position) const {
            return (position.x > TL.x && position.x < BR.x && position.y > TL.y && position.y < BR.y);
        };
    };

    struct Color {
        Color(int r, int g, int b) : r((float)r / 255), g((float)g / 255), b((float)b / 255) {}

        Color(float r, float g, float b) : r(r), g(g), b(b) {}

        Color() : r(1), g(1), b(1) {}

        Color(std::string hex);

        float r;
        float g;
        float b;

        Color operator+(const Color &other) const;
        Color operator-(const Color &other) const;
        Color Inverted();
    };

   private:
    enum UIType { TEXT,
                  BUTTON,
                  IMAGE,
                  VIEW };

    struct UIData {
        UIData(void *ptr, UIType type);

        void *ptr;
        UIType type;
    };

    struct ElementData {
        ElementData() = default;

        Vec2f position;
        Vec2f size;
        int textureId;
        Color color;

        Area2<float> area;

        std::function<void()> onClick;
        std::function<void()> onHoverEnter;
        bool isHovered;
        std::function<void()> onHoverExit;
    };

    struct TextData {
        ElementData base;
        std::string text;
        Font::Font *font;
        std::vector<uint32_t> clusters;
    };

    struct ImageData {
        ElementData base;
    };

    class Element;

    struct ViewData {
        ElementData base;
        std::vector<UIData *> elements;
    };

    class Element {
       protected:
        UIData *data;

       public:
        Element(UIType type) : data(CreateData(type)), position(GetPosition(data)), size(GetSize(data)), id(nextId++), textureId(GetTexture(data)), color(GetColor(data)), area(GetArea(data)), onClick(GetOnClick(data)), onHoverEnter(GetOnHoverEnter(data)), onHoverExit(GetOnHoverExit(data)), isHovered(GetIsHovered(data)) {
            if (!dataPtrs.contains(data)) dataPtrs.insert(data);
        }

        Vec2f &position;
        Vec2f &size;
        int &textureId;
        Color &color;
        std::function<void()> &onClick;
        std::function<void()> &onHoverEnter;
        bool &isHovered;
        std::function<void()> &onHoverExit;
        Area2<float> &area;

        bool Valid() { return data; }

       protected:
        const int id;

        friend class UI;
    };

   public:
    class Text : public Element {
       public:
        Text() : Element(TEXT), text(GetText(data)), font(GetFont(data)), clusters(GetClusters(data)) {}

        Text(Vec2f position, Vec2f size, std::string text, int fontId, Font::TextAlign textAlign = Font::TextAlign::LEFT, Font::TextDirection textDirection = Font::TextDirection::LTR) : Element(TEXT), text(GetText(data)), /* align(textAlign), direction(textDirection),*/ font(GetFont(data)), clusters(GetClusters(data)) {
            this->position = position;
            this->size = size;
            this->text = text;
            this->font = UI::fonts[fontId];
            this->textureId = 0;
            this->color = Color();
        }

        Font::Font *&font;

        // uint32_t FontSize{16};
        // Vec2 bounds{size};
        // Font::TextDirection direction{Font::TextDirection::LTR};
        // Font::TextAlign align{Font::TextAlign::LEFT};
        //  Style style{REGULAR};
        //  Color outline;
        // int outlineThickness{-1};

        std::string &text;
        std::vector<uint32_t> &clusters;

        friend class UI;
    };

   private:
    struct ButtonData {
        ButtonData() : text(Text()) {}

        ElementData base;
        void *function;
        Text text;
    };

   public:
    class Image : public Element {
       private:
        Image(Vec2f position, Vec2f size, Texture texture, Color color, bool dummy) : Element(IMAGE) {
            this->position = position;
            this->size = size;

            this->textureId = texture.id;
            this->color = color;
        }

       public:
        Image(Vec2f position, Vec2f size, Texture texture, Color color) : Image(position, size, texture, color, true) {}

        Image(Vec2f position, Vec2f size, Color color) : Image(position, size, Texture{}, color, true) {}

        Image(Vec2f position, Vec2f size, Texture texture) : Image(position, size, texture, Color(), true) {}

        friend class UI;
    };

    class View : public Element {
       public:
        View(Vec2f position, Vec2f size, int textureId, Color color, bool dummy) : Element(VIEW), elements(GetChildrens(data)) {
            this->position = position;
            this->size = size;
            this->textureId = textureId;
            this->color = color;
        }

        View(Vec2f position, Vec2f size, int textureId, Color color) : View(position, size, textureId, color, true) {}

        View(Vec2f position, Vec2f size, Color color) : View(position, size, 0, color, true) {}

        View(Vec2f position, Vec2f size, int textureId) : View(position, size, textureId, Color(), true) {}

        void Add(Element &element) { elements.push_back(element.data); }

        void Pop(Element &element) {}

       private:
        std::vector<UIData *> &elements;
        friend class UI;
    };

    class Button : public Element {
        Button(Vec2f position, Vec2f size, void *function, std::optional<Text> &text, std::optional<int> textureId, std::optional<Color> color) : Element(BUTTON), function(GetFunction(data)), text(GetTextElement(data)) {
            if (text.has_value()) Bind(this->text, text.value());

            this->position = position;
            this->size = size;

            if (textureId.has_value())
                this->textureId = textureId.value();
            else
                this->textureId = 0;

            if (color.has_value())
                this->color = color.value();
            else
                this->color = Color();
        }

        void *&function;
        Text &text;

        friend class UI;
    };

    static inline void SetMainWindow(Window window) { mainWindow = window; }

    static void Init(Window window);

    static Texture CreateTexture(std::string path);

    static int LoadFont(const std::string &fontPath, uint32_t size = 16);

    template <std::derived_from<UI::Element>... Args>
    static void PushOn(Window window, Args &...args) {
        Render::Surface surface = surfaces[window.ptr];
        if (!Render::IsValidSurface(surface)) return;

        (elements[surface.surface].push_back(args.data), ...);
    }

    static void Submit(Window window = mainWindow);

    static void Bind(Element &dst, Element &src);

    static void Delete(Element &element);

    static void Clean();

    static bool CanDraw();

    static void Draw();

   private:
    static Window mainWindow;
    static std::unordered_map<int, std::vector<UIData *>> elements;
    static int nextId;
    static std::unordered_set<UIData *> dataPtrs;
    static std::unordered_map<int, Font::Font *> fonts;
    static int nextFontId;
    static int pipeline;
    static std::unordered_map<GLFWwindow *, Render::Surface> surfaces;

    struct ProcessData {
        Vec2f ofst;
        Vec2f size;
    };

    static Render::RenderData ProcessVertecies(ProcessData data, std::vector<UIData *> &elements);
    static Render::VertexData GenerateVertecies(UI::ProcessData procData, int textureId, Color color);

    // Data Handling
    static UIData *CreateData(UIType type);
    static void DeleteData(UIData *data);

    // Element
    static Vec2f &GetPosition(UIData *data);
    static Vec2f &GetSize(UIData *data);
    static int &GetTexture(UIData *data);
    static Color &GetColor(UIData *data);
    static Area2<float> &GetArea(UIData *data);
    static std::function<void()> &GetOnClick(UIData *data);
    static std::function<void()> &GetOnHoverEnter(UIData *data);
    static std::function<void()> &GetOnHoverExit(UIData *data);
    static bool &GetIsHovered(UIData *data);

    // Text
    static std::string &GetText(UIData *data);
    static Font::Font *&GetFont(UIData *data);
    static std::vector<uint32_t> &GetClusters(UIData *data);

    static Render::VertexData GenerateTextVertecies(UI::ProcessData procData, UIData *data, UI::Color color);

    // View
    static std::vector<UIData *> &GetChildrens(UIData *data);

    // Button
    static void *&GetFunction(UIData *data);
    static Text &GetTextElement(UIData *data);

    static void HandleClick(Window window);
    static UIData *FindFirstClicked(std::vector<UIData *> &elements, Vec2<float> &position);

    static void HandleCursorMove(Window window);
    static void ProcHoveredElements(std::vector<UIData *> &elements, Vec2<float> &position);

    friend class Input;
};

#ifdef IGNIS_UI_NAMES
using Color = UI::Color;
using Image = UI::Image;
using Text = UI::Text;
using View = UI::View;
using Button = UI::Button;
#endif

#endif

#if defined(IGNIS_INPUT) || defined(IGNIS_UI)
#if defined(IGNIS_UI) && !defined(IGNIS_INPUT)
#define IGNIS_INPUT
#endif
class Input {
   public:
    typedef void (*HookFunction)(Window);

    struct Keydata {
        int key;
        int scancode;
        int action;
        int mods;
    };
    struct ModifierData {
        unsigned int codepoint;
        int mods;
    };
    struct MouseData {
        int button;
        int action;
        int mods;
        bool entered;
    };
    struct DropData {
        int count;
        const char **paths;
    };

   private:
    enum class Callbacks : int;

    using Action = std::pair<GLFWwindow *, Callbacks>;
    using Callback = HookFunction;

    struct KeyHash {
        size_t operator()(const Action &k) const noexcept {
            return std::hash<GLFWwindow *>()(k.first) ^ (std::hash<int>()(static_cast<int>(k.second)) << 1);
        }
    };

    struct WindowInputData {
        Vec2i windowPosition;
        Vec2i windowSize;
        Vec2i framebufferSize;

        Keydata Key;
        unsigned int Char;
        ModifierData modChar;
        MouseData Mouse;
        Vec2<double> cursorPosition;
        Vec2<double> scrollOffset;
        DropData Drop;
    };

    static std::unordered_map<Action, Callback, KeyHash> customCallbacks;
    static std::unordered_map<GLFWwindow *, WindowInputData> windowCallbackData;

    static void WindowPosCallback(GLFWwindow *window, int xpos, int ypos);
    static void WindowSizeCallback(GLFWwindow *window, int width, int height);
    static void WindowCloseCallback(GLFWwindow *window);
    static void WindowRefreshCallback(GLFWwindow *window);
    static void WindowFocusCallback(GLFWwindow *window, int focused);
    static void WindowIconifyCallback(GLFWwindow *window, int iconified);
    static void WindowMaximizeCallback(GLFWwindow *window, int maximized);
    static void FramebufferSizeCallback(GLFWwindow *window, int width, int height);
    // static void WindowContentScaleCallback(GLFWwindow* window, float xscale, float yscale);

    static void InputKeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
    static void InputCharCallback(GLFWwindow *window, unsigned int codepoint);
    static void InputCharModsCallback(GLFWwindow *window, unsigned int codepoint, int mods);
    static void InputMouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
    static void InputCursorPositionCallback(GLFWwindow *window, double xpos, double ypos);
    static void InputCursorEnterCallback(GLFWwindow *window, int entered);
    static void InputScrollCallback(GLFWwindow *window, double xoffset, double yoffset);
    static void InputDropCallback(GLFWwindow *window, int count, const char **paths);

    // static void MonitorCallback(Render::Window window, void* function);

    // static void ErrorCallback(Render::Window window, void* function);

    static void CallFunction(GLFWwindow *window, Callbacks type);

   public:
    static void Init();
    static void InitWindow(Window &window);
    static void Event();

    static void HookWindowPosCallback(Render::Window window, HookFunction function);
    static void HookWindowSizeCallback(Render::Window window, HookFunction function);
    static void HookWindowCloseCallback(Render::Window window, HookFunction function);
    static void HookWindowRefreshCallback(Render::Window window, HookFunction function);
    static void HookWindowFocusCallback(Render::Window window, HookFunction function);
    static void HookWindowIconifyCallback(Render::Window window, HookFunction function);
    static void HookWindowMaximizeCallback(Render::Window window, HookFunction function);
    static void HookFramebufferSizeCallback(Render::Window window, HookFunction function);
    // static void HookWindowContentScaleCallback(Render::Window window, HookFunction function);

    static void HookInputKeyCallback(Render::Window window, HookFunction function);
    static void HookInputCharCallback(Render::Window window, HookFunction function);
    static void HookInputCharModsCallback(Render::Window window, HookFunction function);
    static void HookInputMouseButtonCallback(Render::Window window, HookFunction function);
    static void HookInputCursorPositionCallback(Render::Window window, HookFunction function);
    static void HookInputCursorEnterCallback(Render::Window window, HookFunction function);
    static void HookInputScrollCallback(Render::Window window, HookFunction function);
    static void HookInputDropCallback(Render::Window window, HookFunction function);

    // static void HookMonitorCallback(Render::Window window, void* function);

    // static void HookErrorCallback(Render::Window window, void* function);

    static Vec2i WindowPosition(Render::Window window);
    static Vec2i WindowSize(Render::Window window);
    static Vec2i FramebufferSize(Render::Window window);
    static Keydata Key(Render::Window window);
    static unsigned int Char(Render::Window window);
    static ModifierData ModChar(Render::Window window);
    static MouseData Mouse(Render::Window window);
    static Vec2<double> CursorPosition(Render::Window window);
    static Vec2<double> Scroll(Render::Window window);
    static DropData Drop(Render::Window window);
};
#endif
}  // namespace Ignis
