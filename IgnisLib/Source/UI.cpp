#include <cmath>

#include "Font.h"
#include "IgnisLib.h"

using Vertex = Ignis::Render::Vertex;
using Color = Ignis::UI::Color;

#include "GLFW/glfw3.h"

namespace Ignis {

struct UniformData {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

// Color

auto hexToFloat = [](const std::string &s) -> float { return static_cast<float>(std::stoi(s, nullptr, 16)) / 255.0f; };

Color::Color(std::string hex) {
    if (hex.empty() || hex[0] != '#' || hex.length() != 7) throw std::invalid_argument("Invalid hex color");

    r = hexToFloat(hex.substr(1, 2));
    g = hexToFloat(hex.substr(3, 2));
    b = hexToFloat(hex.substr(5, 2));
}

Color Color::operator+(const Color &other) const { return Color(std::clamp(this->r + other.r, 0.0f, 1.0f), std::clamp(this->g + other.g, 0.0f, 1.0f), std::clamp(this->b + other.b, 0.0f, 1.0f)); }
Color Color::operator-(const Color &other) const { return Color(std::clamp(this->r - other.r, 0.0f, 1.0f), std::clamp(this->g - other.g, 0.0f, 1.0f), std::clamp(this->b - other.b, 0.0f, 1.0f)); }
Color Color::Inverted() { return Color(1.0f - this->r, 1.0f - this->g, 1.0f - this->b); }

// UI Handling
void UI::Init(Window window) {
    if (!windows.contains(window.ptr))
        windows.insert(window.ptr);
}

Render::Texture UI::CreateTexture(std::string path) {
    return Render::CreateTexture(path);
}

bool UI::CanDraw() {
    if (elements.size() == 0) return false;

    bool haveValid = false;
    for (auto &window : windows) {
        haveValid |= Render::IsValidWindow(Window{ window });
    }

    return haveValid;
}

float time = 0.0f;
/*
void UI::Draw() {
    time += 0.01f;
    //Render::PushConstant<float>("off", 0.5 * sin(time));
    //Render::PushConstant<float>("color", fmod(time * 0.2, 1.0f));
    //Render::PushConstant<glm::mat4>("rot", glm::rotate(glm::identity<glm::mat4>(), 3 * time, glm::vec3(0, 0, 1)));
    for (auto &window : windows) {
        //if (Render::IsValidWindow(window))
            //Render::Draw(surface);
    }
}*/

// Data Handling

UI::UIData::UIData(void *ptr, UIType type) : ptr(ptr), type(type) {}

void UI::Delete(Element &element) {
    dataPtrs.erase(element.data);
    DeleteData(element.data);
}

void UI::CleanUp() {
    for (auto &dataPtr : dataPtrs) {
        if (dataPtr) DeleteData(dataPtr);
    }

    dataPtrs.clear();

    for (auto &[key, fontPtr] : fonts) {
        delete fontPtr;
    }
    std::cout << "UI data has been freed" << std::endl;
}

UI::UIData *UI::CreateData(UIType type) {
    UIData *data;

    switch (type) {
        case Ignis::UI::TEXT:
            data = new UIData(new TextData, TEXT);
            break;
        case Ignis::UI::BUTTON:
            data = new UIData(new ButtonData, BUTTON);
            break;
        case Ignis::UI::IMAGE:
            data = new UIData(new ImageData, IMAGE);
            break;
        case Ignis::UI::VIEW:
            data = new UIData(new ViewData, VIEW);
            break;
        default:
            throw std::runtime_error("Coudn't create data for the said type");
            break;
    }

    return data;
}
void UI::DeleteData(UIData *data) {
    switch (data->type) {
        case Ignis::UI::TEXT:
            delete static_cast<TextData *>(data->ptr);
            break;
        case Ignis::UI::BUTTON:
            delete static_cast<ButtonData *>(data->ptr);
            break;
        case Ignis::UI::IMAGE:
            delete static_cast<ImageData *>(data->ptr);
            break;
        case Ignis::UI::VIEW:
            delete static_cast<ViewData *>(data->ptr);
            break;
        default:
            throw std::runtime_error("Coudn't clean up UIData pointer");
    }

    data->ptr = nullptr;

    delete data;
    data = nullptr;
}

void UI::Bind(Element &dst, Element &src) {
    if (dst.data->type != src.data->type) throw std::runtime_error("Tried to bind together two different type of element");

    DeleteData(dst.data);
    dst.data = src.data;

    switch (src.data->type) {
        case TEXT: {
            Text &textDst = static_cast<Text &>(dst);
            textDst.position = GetPosition(dst.data);
            textDst.size = GetSize(dst.data);
            textDst.text = GetText(dst.data);
            break;
        }
        case BUTTON: {
            Button &btnDst = static_cast<Button &>(dst);
            btnDst.position = GetPosition(dst.data);
            btnDst.size = GetSize(dst.data);
            btnDst.color = GetColor(dst.data);
            btnDst.textureId = GetTexture(dst.data);
            btnDst.function = GetFunction(dst.data);
            Bind(btnDst.text, GetTextElement(dst.data));
            break;
        }
        case IMAGE: {
            Image &imgDst = static_cast<Image &>(dst);
            imgDst.position = GetPosition(dst.data);
            imgDst.size = GetSize(dst.data);
            imgDst.color = GetColor(dst.data);
            imgDst.textureId = GetTexture(dst.data);
            break;
        }
        case VIEW: {
            View &viewDst = static_cast<View &>(dst);
            viewDst.position = GetPosition(dst.data);
            viewDst.size = GetSize(dst.data);
            viewDst.color = GetColor(dst.data);
            viewDst.textureId = GetTexture(dst.data);
            viewDst.elements = GetChildrens(dst.data);
            break;
        }
    }
}

void UI::Submit(Window &window) {
    if (!elements.contains(window.ptr)) return;

    UI::ProcessData data{ .ofst{ 0, 0 }, .size{ 2, 2 } };

    Render::RenderData outputData;
    GenerateInstanceVertecies(outputData);
    ProcessElements(outputData, data, elements[window.ptr]);
    outputData.window = window;
    outputData.changed = true;
    Render::AddUIRenderData(outputData);
}

glm::mat4 createModel(glm::vec3 position, glm::vec3 scale, float rotation, glm::vec3 axis) {
    glm::mat4 model = glm::mat4(1.0f);

    model = glm::translate(model, position);
    model = glm::rotate(model, glm::radians(rotation), axis);
    model = glm::scale(model, scale);

    return model;
}

// uint32_t normal = (127 << 16) | (127 << 8) | (255);

void UI::ProcessElements(Render::RenderData &returnData, UI::ProcessData data, std::vector<UIData *> &elements) {
    for (auto element : elements) {
        Vec2f &position = GetPosition(element);
        Vec2f &size = GetSize(element);

        Vec2f elementSize = { data.size.x / 100 * size.x, data.size.y / 100 * size.y };
        Vec2f elementOffset = { data.size.x / 100 * position.x, data.size.y / 100 * position.y };

        UI::ProcessData calcData{ .ofst{ data.ofst.x + elementOffset.x, data.ofst.y + elementOffset.y }, .size{ elementSize } };

        Area2<float> &area = GetArea(element);
        area = Area2<float>(Vec2<float>(calcData.ofst.x / 2, calcData.ofst.y / 2),
                            Vec2<float>((calcData.ofst.x + calcData.size.x) / 2, (calcData.ofst.y + calcData.size.y) / 2));

        if (element->type == TEXT) {
            // TODO: the council shall decide its fate
        } else {
            Render::InstanceData instance;

            instance.model = createModel(
                glm::vec3(-1 + calcData.ofst.x, -1 + calcData.ofst.y, 0.0f),  // position
                glm::vec3(calcData.size.x, calcData.size.y, 1.0f),            // scale
                0.0f,                                                         // rotation in degrees
                glm::vec3(0.0f, 0.0f, 1.0f)                                   // axis
            );

            instance.textureId = (uint32_t)GetTexture(element);

            Color colorIn = GetColor(element);
            instance.color = ((char)(colorIn.r * 265) << 16) | ((char)(colorIn.g * 265) << 16) | ((char)(colorIn.b * 265) << 16);

            returnData.instances.push_back(instance);
        }

        if (element->type == VIEW) {
            auto view = static_cast<ViewData *>(element->ptr);
            UI::ProcessElements(returnData, calcData, view->elements);
        } else if (element->type == BUTTON) {
            auto button = static_cast<ButtonData *>(element->ptr);
            std::vector<UIData *> text = { button->text.data };
            UI::ProcessElements(returnData, calcData, text);
        }
    }
}

void UI::GenerateInstanceVertecies(Render::RenderData &renderData) {
    Render::InstanceVertex topLeft = { glm::vec3(-0.5f, -0.5f, 0.0f) };
    Render::InstanceVertex topRight = { glm::vec3(0.5f, -0.5f, 0.0f) };
    Render::InstanceVertex bottomRight = { glm::vec3(0.5f, 0.5f, 0.0f) };
    Render::InstanceVertex bottomLeft = { glm::vec3(-0.5f, 0.5f, 0.0f) };

    renderData.vertecies = { topLeft, topRight, bottomRight, bottomLeft };
    renderData.indicies = { 0, 2, 1, 0, 3, 2 };
}
/*
// TODO: change the whole position and sizing shit
Render::VertexData UI::GenerateTextVertecies(UI::ProcessData procDt, UIData *data, UI::Color colorIn, uint32_t normal) {
    // if (data->type != TEXT) return;
    // TextData* textData = static_cast<TextData*>(data->ptr);

    // TODO: unhardcode it IMPORTANT
    float hardcode = 2.0f;
    Render::VertexData returnData;
    uint32_t color = ((char)(colorIn.r * 265) << 16) | ((char)(colorIn.g * 265) << 16) | ((char)(colorIn.b * 265) << 16);
    glm::vec2 norm(100);

    if (!GetFont(data) || GetText(data).empty()) {
        std::cout << "sad\n";
        std::cout << "text: " << GetText(data) << "\n";
        return {};
    }

    Font::Font *font = GetFont(data);
    std::string &text = GetText(data);

    glm::vec2 pen(procDt.ofst.x, procDt.ofst.y);
    float scale = 1.0f / (64.0f * norm.x * hardcode);

    auto glyphs = font->shapeText(Font::sToU32s(text), font->defaultSize, Font::TextAlign::GUESS, Font::TextDirection::GUESS);

    uint32_t indiceOffset = 0;
    float baseline = font->defaultSize;
    for (auto &sg : glyphs) {
        const Font::Glyph *g = sg.glyph;
        if (g) {
            // Position: pen + shaped offsets
            glm::vec2 glyphPos = pen + glm::vec2((float)(sg.xOffset + sg.getLeft()), baseline + ((float)(-sg.yOffset + (font->defaultSize * 64 - g->bearingY)))) * scale;
            // Size: from glyph rect, scaled
            glm::vec2 glyphSize = glm::vec2(g->w, g->h) / norm / hardcode;

            // std::cout << "values\n";
            // std::cout << (char)g->getUnicode() << " glyph\n";
            // std::cout << "scale: " << scale << "\n";
            // std::cout << "pos: ("<<glyphPos.x << "," << glyphPos.y <<")\n";
            // std::cout << "size: ("<<glyphSize.x << "," << glyphSize.y <<")\n";
            // std::cout << "g size: (" << g->w << "," << g->h << ")\n";
            // std::cout << "uv: ("<<g->u0 << "," << g->v0 <<"),("<<g->u1 << "," << g->v1 << ")\n";
            // std::cout << "pId: " << sg.pId << "\n";
            // std::cout << "bearing: (" << g->bearingX << "," << g->bearingY << ")\n";
            // std::cout << "advance: (" << g->advanceX << "," << g->advanceY << ")\n";
            // std::cout << "pen: (" << pen.x << "," << pen.y << ")\n";
            uint32_t pageId = sg.pId;

            Vertex topLeft = { glm::vec3(-1 + glyphPos.x, -1 + glyphPos.y, 0.0f), normal, glm::vec2(g->u0, g->v0), color };
            Vertex topRight = { glm::vec3(-1 + glyphPos.x + glyphSize.x, -1 + glyphPos.y, 0.0f), normal, glm::vec2(g->u1, g->v0), color };
            Vertex bottomRight = { glm::vec3(-1 + glyphPos.x + glyphSize.x, -1 + glyphPos.y + glyphSize.y, 0.0f), normal, glm::vec2(g->u1, g->v1), color };
            Vertex bottomLeft = { glm::vec3(-1 + glyphPos.x, -1 + glyphPos.y + glyphSize.y, 0.0f), normal, glm::vec2(g->u0, g->v1), color };
            // std::cout << "top-left: (" << topLeft.pos.x << "," << topLeft.pos.y << ")\n";
            // std::cout << "top-right: (" << topRight.pos.x << "," << topRight.pos.y << ")\n";
            // std::cout << "bottom-right:" << bottomRight.pos.x << "," << bottomRight.pos.y << ")\n";
            // std::cout << "bottom-left:" << bottomLeft.pos.x << "," << bottomLeft.pos.y << ")\n";

            returnData.vertecies.insert(returnData.vertecies.end(), { topLeft, topRight, bottomRight, bottomLeft });

            returnData.indicies.insert(returnData.indicies.end(), { 0 + indiceOffset, 2 + indiceOffset, 1 + indiceOffset, 0 + indiceOffset, 3 + indiceOffset, 2 + indiceOffset });

            indiceOffset += 4;
        }
        pen.x += (float)sg.xAdvance * scale;
    }
    return returnData;
}*/

UI::UIData *UI::FindFirstClicked(std::vector<UIData *> &elements, Vec2<float> &position) {
    UIData *clickedElement = nullptr;

    // loops from newer to older and checks if it can be more precise with a smoller object
    for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
        auto &element = *it;
        if (GetArea(element).Contains(position)) {
            if (element->type == VIEW) {
                clickedElement = FindFirstClicked(GetChildrens(element), position);
                if (clickedElement != nullptr) return clickedElement;
            }
            auto &func = GetOnClick(element);
            if (func) {
                func();
                return element;
            }
        }
    }

    return clickedElement;
}

void UI::HandleClick(Window window) {
    Vec2i windowSize = Input::WindowSize(window);
    Vec2<double> cursorPositoin = Input::CursorPosition(window);
    Vec2<float> position = Vec2<float>(cursorPositoin.x / (float)windowSize.x, cursorPositoin.y / (float)windowSize.y);

    Input::MouseData mouse = Input::Mouse(window);
    if (mouse.action == GLFW_PRESS && mouse.button == GLFW_MOUSE_BUTTON_LEFT) {
        UIData *clickedElement;

        // checks every surface on the position
        for (auto &element : elements) {
            if (element.first == window.ptr) {
                clickedElement = FindFirstClicked(element.second, position);
                break;
            }
        }

        if (clickedElement != nullptr) {
            std::cout << "Something clicked at: " << position.x << "; " << position.y << std::endl;
            std::cout << "Type: " << clickedElement->type << std::endl;
        } else {
            std::cout << "Did not click shit" << std::endl;
        }
    }
}

void UI::ProcHoveredElements(std::vector<UIData *> &elements, Vec2<float> &position) {
    // loops from newer to older and checks if it can be more precise with a smoller object
    for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
        auto &element = *it;
        bool &isHovered = GetIsHovered(element);
        if (GetArea(element).Contains(position)) {
            if (!isHovered) {
                auto &func = GetOnHoverEnter(element);
                if (func) func();
                isHovered = true;
            }
        } else {
            if (isHovered) {
                auto &func = GetOnHoverExit(element);
                if (func) func();
                isHovered = false;
            }
        }
        if (element->type == VIEW) {
            ProcHoveredElements(GetChildrens(element), position);
        }
    }
}

void UI::HandleCursorMove(Window window) {
    Vec2i windowSize = Input::WindowSize(window);
    Vec2<double> cursorPositoin = Input::CursorPosition(window);
    Vec2<float> position = Vec2<float>(cursorPositoin.x / (float)windowSize.x, cursorPositoin.y / (float)windowSize.y);

    for (auto &element : elements) {
        if (element.first == window.ptr) {
            ProcHoveredElements(element.second, position);
            break;
        }
    }
}

int UI::LoadFont(const std::string &fontPath, uint32_t size) {
    Font::Font *font = new Font::Font();
    font->defaultSize = size;
    font->initializeFont(fontPath);
    std::cout << "font initialized\n";

    fonts[nextFontId] = font;
    // I want the old value so post-increment is correct
    return nextFontId++;
}

// Element

Vec2f &UI::GetPosition(UIData *data) {
    switch (data->type) {
        case TEXT:
            return static_cast<TextData *>(data->ptr)->base.position;
        case IMAGE:
            return static_cast<ImageData *>(data->ptr)->base.position;
        case VIEW:
            return static_cast<ViewData *>(data->ptr)->base.position;
        case BUTTON:
            return static_cast<ButtonData *>(data->ptr)->base.position;
        default:
            throw std::runtime_error("Coudn't access position of the passed in data");
    }
}

Vec2f &UI::GetSize(UIData *data) {
    switch (data->type) {
        case TEXT:
            return static_cast<TextData *>(data->ptr)->base.size;
        case IMAGE:
            return static_cast<ImageData *>(data->ptr)->base.size;
        case VIEW:
            return static_cast<ViewData *>(data->ptr)->base.size;
        case BUTTON:
            return static_cast<ButtonData *>(data->ptr)->base.size;
        default:
            throw std::runtime_error("Coudn't access size of the passed in data");
    }
}

Color &UI::GetColor(UIData *data) {
    switch (data->type) {
        case IMAGE:
            return static_cast<ImageData *>(data->ptr)->base.color;
        case VIEW:
            return static_cast<ViewData *>(data->ptr)->base.color;
        case BUTTON:
            return static_cast<ButtonData *>(data->ptr)->base.color;
        case TEXT:
            return static_cast<TextData *>(data->ptr)->base.color;
        default:
            throw std::runtime_error("Coudn't access color of the passed in data");
    }
}

int &UI::GetTexture(UIData *data) {
    switch (data->type) {
        case IMAGE:
            return static_cast<ImageData *>(data->ptr)->base.textureId;
        case VIEW:
            return static_cast<ViewData *>(data->ptr)->base.textureId;
        case BUTTON:
            return static_cast<ButtonData *>(data->ptr)->base.textureId;
        case TEXT:
            return static_cast<TextData *>(data->ptr)->base.textureId;
        default:
            throw std::runtime_error("Coudn't access texture of the passed in data");
    }
}

UI::Area2<float> &UI::GetArea(UIData *data) {
    switch (data->type) {
        case IMAGE:
            return static_cast<ImageData *>(data->ptr)->base.area;
        case VIEW:
            return static_cast<ViewData *>(data->ptr)->base.area;
        case BUTTON:
            return static_cast<ButtonData *>(data->ptr)->base.area;
        case TEXT:
            return static_cast<TextData *>(data->ptr)->base.area;
        default:
            throw std::runtime_error("Coudn't access texture of the passed in data");
    }
}

std::function<void()> &UI::GetOnClick(UIData *data) {
    switch (data->type) {
        case IMAGE:
            return static_cast<ImageData *>(data->ptr)->base.onClick;
        case VIEW:
            return static_cast<ViewData *>(data->ptr)->base.onClick;
        case TEXT:
            return static_cast<TextData *>(data->ptr)->base.onClick;
        default:
            throw std::runtime_error("Coudn't access texture of the passed in data");
    }
}

std::function<void()> &UI::GetOnHoverEnter(UIData *data) {
    switch (data->type) {
        case IMAGE:
            return static_cast<ImageData *>(data->ptr)->base.onHoverEnter;
        case VIEW:
            return static_cast<ViewData *>(data->ptr)->base.onHoverEnter;
        case TEXT:
            return static_cast<TextData *>(data->ptr)->base.onHoverEnter;
        default:
            throw std::runtime_error("Coudn't access texture of the passed in data");
    }
}

std::function<void()> &UI::GetOnHoverExit(UIData *data) {
    switch (data->type) {
        case IMAGE:
            return static_cast<ImageData *>(data->ptr)->base.onHoverExit;
        case VIEW:
            return static_cast<ViewData *>(data->ptr)->base.onHoverExit;
        case TEXT:
            return static_cast<TextData *>(data->ptr)->base.onHoverExit;
        default:
            throw std::runtime_error("Coudn't access texture of the passed in data");
    }
}

bool &UI::GetIsHovered(UIData *data) {
    switch (data->type) {
        case IMAGE:
            return static_cast<ImageData *>(data->ptr)->base.isHovered;
        case VIEW:
            return static_cast<ViewData *>(data->ptr)->base.isHovered;
        case TEXT:
            return static_cast<TextData *>(data->ptr)->base.isHovered;
        default:
            throw std::runtime_error("Coudn't access texture of the passed in data");
    }
}

// Text

std::string &UI::GetText(UIData *data) {
    switch (data->type) {
        case TEXT:
            return static_cast<TextData *>(data->ptr)->text;
            break;
        default:
            throw std::runtime_error("Coudn't access text of the passed in data");
    }
}

Font::Font *&UI::GetFont(UIData *data) {
    switch (data->type) {
        case TEXT:
            return static_cast<TextData *>(data->ptr)->font;
            break;
        default:
            throw std::runtime_error("Coudn't access text of the passed in data");
    }
}

std::vector<uint32_t> &UI::GetClusters(UIData *data) {
    switch (data->type) {
        case TEXT:
            return static_cast<TextData *>(data->ptr)->clusters;
            break;
        default:
            throw std::runtime_error("Coudn't access text of the passed in data");
    }
}

// View

std::vector<UI::UIData *> &UI::GetChildrens(UIData *data) {
    switch (data->type) {
        case VIEW:
            return static_cast<ViewData *>(data->ptr)->elements;
        default:
            throw std::runtime_error("Coudn't access childrends of the passed in data");
    }
}

// Button

void *&UI::GetFunction(UIData *data) {
    switch (data->type) {
        case BUTTON:
            return static_cast<ButtonData *>(data->ptr)->function;
        default:
            throw std::runtime_error("Coudn't access function of the passed in data");
    }
}

UI::Text &UI::GetTextElement(UIData *data) {
    switch (data->type) {
        case BUTTON:
            return static_cast<ButtonData *>(data->ptr)->text;
        default:
            throw std::runtime_error("Coudn't access Text of the passed in data");
    }
}

// Variables

Window UI::mainWindow;
std::unordered_map<int, Font::Font *> UI::fonts;
int UI::nextId = 0;
int UI::nextFontId = 1;
std::unordered_map<GLFWwindow *, std::vector<UI::UIData *>> UI::elements;
std::unordered_set<UI::UIData *> UI::dataPtrs;
int UI::pipeline = true;
std::unordered_set<GLFWwindow *> UI::windows;
}  // namespace Ignis
