#include "Font.h"
#include "IgnisLib.h"

using Vertex = Ignis::Render::Vertex;
using Color = Ignis::UI::Color;

namespace Ignis {

// Vector

// Area
template <typename T>
    requires std::is_arithmetic_v<T>
inline void UI::Area2<T>::Calc() {
    TR = (BR.x, TL.y);
    BL = (TL.x, BR.y);
}

template <typename T>
    requires std::is_arithmetic_v<T>
inline bool UI::Area2<T>::Contain(Vec2<T> &position) const {
    return (position.x > TL.x && position.x < BR.x && position.y > TL.y && position.y < BR.y);
}

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

// Data Handling

UI::UIData::UIData(void *ptr, UIType type) : ptr(ptr), type(type) {}

void UI::Delete(Element &element) {
    dataPtrs.erase(element.data);
    DeleteData(element.data);
}

void UI::Clean() {
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
            data = new UIData(new ButtonData{ .text = Text() }, BUTTON);
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

void UI::SubmitSurface(int surface) {
    if (!elements.contains(surface)) return;

    GLFWwindow* window = static_cast<GLFWwindow*>(Render::GetWindowOfSurface(surface));

    UI::ProcessData data{ .ofst{ 0, 0 }, .size{ 2, 2 } };

    Render::UIRenderData outputData = ProcessVertecies(data, elements[surface]);
    outputData.surface = surface;
    outputData.changed = true;
    Render::AddUIElementData(outputData);
}

Render::UIRenderData UI::ProcessVertecies(UI::ProcessData data, std::vector<Element> &elements) {
    Render::UIRenderData returnData;

    int additionIndex = 0;

    for (auto &element : elements) {
        Vec2f elementSize = { data.size.x / 100 * element.size.x, data.size.y / 100 * element.size.y };
        Vec2f elementOffset = { data.size.x / 100 * element.position.x, data.size.y / 100 * element.position.y };

        UI::ProcessData calcData{ .ofst{ data.ofst.x + elementOffset.x, data.ofst.y + elementOffset.y }, .size{ elementSize } };
        if (element.data->type == TEXT) {
            UIVertexData textVertexData = GenerateTextVertecies(calcData, element.data, GetColor(element.data));

            returnData.vertecies.insert(returnData.vertecies.end(), textVertexData.vertecies.begin(), textVertexData.vertecies.end());

            // fun word
            for (auto indicy : textVertexData.indicies) {
                returnData.indicies.push_back(indicy + additionIndex);
            }
            additionIndex += textVertexData.vertecies.size();
        } else {
            UI::UIVertexData vertexData = GenerateVertecies(calcData, element.textureId, element.color);

            returnData.vertecies.insert(returnData.vertecies.end(), vertexData.vertecies.begin(), vertexData.vertecies.end());

            // i wont look it up how they write it, I BELIIIIVEEEEE
            for (auto &indicy : vertexData.indicies) {
                returnData.indicies.push_back(indicy + additionIndex);
            }
            additionIndex += 4;
        }

        Render::UIRenderData childData;

        if (element.data->type == VIEW) {
            auto &view = static_cast<View &>(element);
            childData = UI::ProcessVertecies(calcData, *view.elements);
        } else if (element.data->type == BUTTON) {
            auto &button = static_cast<Button &>(element);
            std::vector<Element> text = { button.text };
            childData = UI::ProcessVertecies(calcData, text);
        }

        if (childData.vertecies.size() > 0) {
            returnData.vertecies.insert(returnData.vertecies.end(), childData.vertecies.begin(), childData.vertecies.end());

            for (auto &indicy : childData.indicies) {
                returnData.indicies.push_back(indicy + additionIndex);
            }

            additionIndex += childData.vertecies.size();
        }
    }

    return returnData;
}

UI::UIVertexData UI::GenerateVertecies(UI::ProcessData procDt, int textureId, Color color) {
    glm::vec3 vertexColor = glm::vec3(color.r, color.g, color.b);
    glm::uint texture = glm::uint(textureId);
    Vertex topLeft = { glm::vec3(-1 + procDt.ofst.x, -1 + procDt.ofst.y, 0.0f), vertexColor, glm::vec2(0.0f, 0.0f), texture };
    Vertex topRight = { glm::vec3(-1 + procDt.ofst.x + procDt.size.x, -1 + procDt.ofst.y, 0.0f), vertexColor, glm::vec2(1.0f, 0.0f), texture };
    Vertex bottomRight = { glm::vec3(-1 + procDt.ofst.x + procDt.size.x, -1 + procDt.ofst.y + procDt.size.y, 0.0f), vertexColor, glm::vec2(1.0f, 1.0f), texture };
    Vertex bottomLeft = { glm::vec3(-1 + procDt.ofst.x, -1 + procDt.ofst.y + procDt.size.y, 0.0f), vertexColor, glm::vec2(0.0f, 1.0f), texture };

    UIVertexData returnData{ .vertecies = { topLeft, topRight, bottomRight, bottomLeft },
                             .indicies = { 0, 2, 1, 0, 3, 2 } };
    return returnData;
}

// TODO: change the whole position and sizing shit
UI::UIVertexData UI::GenerateTextVertecies(UI::ProcessData procDt, UIData *data, UI::Color color) {
    // if (data->type != TEXT) return;
    // TextData* textData = static_cast<TextData*>(data->ptr);

    // TODO: unhardcode it IMPORTANT
    float hardcode = 2.0f;
    UIVertexData returnData;
    glm::vec3 vertexColor = glm::vec3((float)color.r, (float)color.g, (float)color.b);
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

            Vertex topLeft = { glm::vec3(-1 + glyphPos.x, -1 + glyphPos.y, 0.0f), vertexColor, glm::vec2(g->u0, g->v0), pageId };
            Vertex topRight = { glm::vec3(-1 + glyphPos.x + glyphSize.x, -1 + glyphPos.y, 0.0f), vertexColor, glm::vec2(g->u1, g->v0), pageId };
            Vertex bottomRight = { glm::vec3(-1 + glyphPos.x + glyphSize.x, -1 + glyphPos.y + glyphSize.y, 0.0f), vertexColor, glm::vec2(g->u1, g->v1), pageId };
            Vertex bottomLeft = { glm::vec3(-1 + glyphPos.x, -1 + glyphPos.y + glyphSize.y, 0.0f), vertexColor, glm::vec2(g->u0, g->v1), pageId };
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

// Image

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

// View

std::vector<UI::Element> *UI::GetChildrens(UIData *data) {
    switch (data->type) {
        case VIEW:
            return &static_cast<ViewData *>(data->ptr)->elements;
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

int UI::mainSurface = -1;
std::unordered_map<int, Font::Font *> UI::fonts;
int UI::nextId = 0;
int UI::nextFontId = 1;
std::unordered_map<int, std::vector<UI::Element>> UI::elements;
std::unordered_set<UI::UIData *> UI::dataPtrs;
}  // namespace Ignis
