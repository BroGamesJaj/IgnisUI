#include "IgnisLib.h"

#include "Font.h"

using Vec2 = Ignis::UI::Vec2;
using Vertex = Ignis::Render::Vertex;
using Color = Ignis::UI::Color;

namespace Ignis {

	//Data Handling

	UI::UIData::UIData(void* ptr, UIType type) : ptr(ptr), type(type) {}

	void UI::Delete(Element& element) {
		dataPtrs.erase(element.data);
		DeleteData(element.data);
	}

	void UI::Clean() {
		for (auto& dataPtr : dataPtrs) {
			if (dataPtr)
				DeleteData(dataPtr);
		}

		dataPtrs.clear();

        for(auto &[key, fontPtr] : fonts){
            delete fontPtr;
        }
		std::cout << "UI data has been freed" << std::endl;
	}

	UI::UIData* UI::CreateData(UIType type) {
		UIData* data;

		switch (type)
		{
		case Ignis::UI::TEXT:
			data = new UIData(new TextData, TEXT);
			break;
		case Ignis::UI::BUTTON:
			data = new UIData(new ButtonData{.text = Text()}, BUTTON);
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
	void UI::DeleteData(UIData* data) {
		switch (data->type)
		{
		case Ignis::UI::TEXT:
			delete static_cast<TextData*>(data->ptr);
			break;
		case Ignis::UI::BUTTON:
			delete static_cast<ButtonData*>(data->ptr);
			break;
		case Ignis::UI::IMAGE:
			delete static_cast<ImageData*>(data->ptr);
			break;
		case Ignis::UI::VIEW:
			delete static_cast<ViewData*>(data->ptr);
			break;
		default:
			throw std::runtime_error("Coudn't clean up UIData pointer");
		}

		data->ptr = nullptr;

		delete data;
		data = nullptr;
	}

	void UI::Bind(Element& dst, Element& src) {
		if (dst.data->type != src.data->type)
			throw std::runtime_error("Tried to bind together two different type of element");

		DeleteData(dst.data);
		dst.data = src.data;

		switch (src.data->type)
		{
		case TEXT: {
            std::cout << "bind text\n";
			Text& textDst = static_cast<Text&>(dst);
			textDst.position = GetPosition(dst.data);
			textDst.size = GetSize(dst.data);
			textDst.text = GetText(dst.data);
			break;
		}
		case BUTTON: {
			Button& btnDst = static_cast<Button&>(dst);
			btnDst.position = GetPosition(dst.data);
			btnDst.size = GetSize(dst.data);
			btnDst.color = GetColor(dst.data);
			btnDst.textureId = GetTexture(dst.data);
			btnDst.function = GetFunction(dst.data);
			Bind(btnDst.text, GetTextElement(dst.data));
			break;
		}
		case IMAGE: {
			Image& imgDst = static_cast<Image&>(dst);
			imgDst.position = GetPosition(dst.data);
			imgDst.size = GetSize(dst.data);
			imgDst.color = GetColor(dst.data);
			imgDst.textureId = GetTexture(dst.data);
			break;
		}
		case VIEW: {
			View& viewDst = static_cast<View&>(dst);
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

		UI::ProcessData data{ .ofst{0,0},.size{2,2} };

		Render::UIRenderData outputData = ProcessVertecies(data, elements[surface]);
		outputData.surface = surface;
		outputData.changed = true;
		renderInstance->AddUIElementData(outputData);
	}

	Render::UIRenderData UI::ProcessVertecies(UI::ProcessData data, std::vector<Element>& elements) {
        std::cout << "ProcessVertecies\n";
		Render::UIRenderData returnData;

		int additionIndex = 0;

		for (auto& element : elements) {
			Vec2 elementSize = { data.size.x / 100 * element.size.x , data.size.y / 100 * element.size.y };
			Vec2 elementOffset = { data.size.x / 100 * element.position.x, data.size.y / 100 * element.position.y };


			UI::ProcessData calcData{ .ofst{data.ofst.x + elementOffset.x, data.ofst.y + elementOffset.y}, .size{elementSize} };
            if (element.data->type == TEXT) {
                std::cout << "text processing\n";
                if (UI::Text* textPtr = dynamic_cast<UI::Text*>(&element)) {
                    std::cout << "good\n";
                } else {
                    std::cout << "bad\n";
                }

                UI::Text &text = dynamic_cast<UI::Text&>(element);
                std::cout << "a text: "<< text.text << "\n";
               
                auto glyphs = GenerateGlyphInstances(text,calcData);
                
                if (!glyphs.empty()) {
                std::cout << "not empty\n";
                    UIVertexData quadData = GenerateTextQuad(text.color);
                    
                    returnData.glyphVertecies.insert(
                        returnData.glyphVertecies.end(),
                        quadData.vertecies.begin(),
                        quadData.vertecies.end()
                    );
                    
                    // fun word
                    for (auto indicy : quadData.indicies) {
                        returnData.glyphIndicies.push_back(indicy + additionIndex);
                    }
                    additionIndex += 4;
                    
                    returnData.glyphInstances.insert(
                        returnData.glyphInstances.end(),
                        glyphs.begin(),
                        glyphs.end()
                    );
                }
            } else {
                UI::UIVertexData vertexData = GenerateVertecies(calcData, element.textureId, element.color);

                returnData.vertecies.insert(
                    returnData.vertecies.end(),
                    vertexData.vertecies.begin(),
                    vertexData.vertecies.end()
                );

                //i wont look it up how they write it, I BELIIIIVEEEEE
                for (auto& indicy : vertexData.indicies) {
                    returnData.indicies.push_back(indicy + additionIndex);
                }
                additionIndex += 4;
            }

			Render::UIRenderData childData;

			if (element.data->type == VIEW) {
				auto& view = static_cast<View&>(element);
				childData = UI::ProcessVertecies(calcData, *view.elements);
			}
			else if (element.data->type == BUTTON) {
				auto& button = static_cast<Button&>(element);
				std::vector<Element> text = { button.text };
				childData = UI::ProcessVertecies(calcData, text);
			}

			if (childData.vertecies.size() > 0) {
				returnData.vertecies.insert(
					returnData.vertecies.end(),
					childData.vertecies.begin(),
					childData.vertecies.end()
				);

				for (auto& indicy : childData.indicies) {
					returnData.indicies.push_back(indicy + additionIndex);
				}

				additionIndex += childData.vertecies.size();
			}
		}

		return returnData;
	}

	UI::UIVertexData UI::GenerateVertecies(UI::ProcessData procDt, int textureId, Color color) {

		glm::vec3 vertexColor = glm::vec3((float)color.r / 255, (float)color.g / 255, (float)color.b / 255);
		glm::uint texture = glm::uint(textureId);

		UIVertexData returnData{
			.vertecies = {
				{ glm::vec3(-1 + procDt.ofst.x, -1 + procDt.ofst.y, 0.0f), vertexColor, glm::vec2(1.0f, 0.0f), texture},
				{ glm::vec3(-1 + procDt.ofst.x + procDt.size.x, -1 + procDt.ofst.y, 0.0f), vertexColor, glm::vec2(1.0f, 1.0f), texture},
				{ glm::vec3(-1 + procDt.ofst.x + procDt.size.x, -1 + procDt.ofst.y + procDt.size.y, 0.0f), vertexColor, glm::vec2(0.0f, 1.0f), texture},
				{ glm::vec3(-1 + procDt.ofst.x, -1 + procDt.ofst.y + procDt.size.y, 0.0f), vertexColor, glm::vec2(0.0f, 1.0f), texture},
			},
			.indicies = {
				0, 2, 1, 0, 3, 2
			}
		};
		return returnData;
	}

    UI::UIVertexData UI::GenerateTextQuad(UI::Color color) {
        glm::vec3 vertexColor(color.r/255.0f, color.g/255.0f, color.b/255.0f);
        glm::uint dummyTexId = 0;  // ignored for text

        // UNIT QUAD (-0.5 to 0.5) - positioning comes from GlyphInstance.pos
        return {
            .vertecies = {
                {glm::vec3(-0.5f,  0.5f, 0.0f), vertexColor, glm::vec2(0.0f, 0.0f), dummyTexId},
                {glm::vec3( 0.5f,  0.5f, 0.0f), vertexColor, glm::vec2(1.0f, 0.0f), dummyTexId},
                {glm::vec3( 0.5f, -0.5f, 0.0f), vertexColor, glm::vec2(1.0f, 1.0f), dummyTexId},
                {glm::vec3(-0.5f, -0.5f, 0.0f), vertexColor, glm::vec2(0.0f, 1.0f), dummyTexId}
            },
            .indicies = {0, 1, 2, 0, 2, 3}
        };
    }
    std::vector<Render::GlyphInstance> UI::GenerateGlyphInstances(UI::Text &text, UI::ProcessData proc) {
        std::vector<Render::GlyphInstance> instances;
    

        if (!text.font || text.text.empty()) {
            std::cout << "sad\n";
            std::cout << "text: " << text.text << "\n";
            return instances;
        }

        glm::vec2 pen(proc.ofst.x, proc.ofst.y);
        float scale = text.FontSize / 64.0f;  // adjust for your font units

        auto glyphs = text.font->shapeText(Font::sToU32s(text.text), text.FontSize, text.bounds.x, text.bounds.y, text.align, text.direction);
        for (auto sg : glyphs) {
            const Font::Glyph* g = sg.glyph;
            if (!g) continue;

            // Position: pen + shaped offsets
            glm::vec2 glyphPos = pen + 
                glm::vec2(sg.xOffset * scale, -sg.yOffset * scale);
            
            // Size: from glyph rect, scaled
            glm::vec2 glyphSize = glm::vec2(g->w, g->h) * scale;

            instances.push_back({
                glyphPos, 
                glyphSize,
                glm::vec4(g->u0, g->v0, g->u1, g->v1), 
                static_cast<glm::uint>(sg.pId)
            });
            
            pen.x += sg.xAdvance * scale;
        }
        std::cout << "instance count: " << instances.size() << "\n";
        return instances;
    }    

    int UI::LoadFont(const std::string& fontPath, uint32_t size) {
        Font::Font *font = new Font::Font();
        font->renderer = renderInstance;
        font->defaultSize = size;
        std::cout << "before init\n";
        font->initializeFont(fontPath);
        std::cout << "font initialized\n";
        
        fonts[nextFontId] = font;
        // I want the old value so post-increment is correct
        return nextFontId++;
    }


	//Element

	Vec2& UI::GetPosition(UIData* data) {
		switch (data->type) {
		case TEXT:
			return static_cast<TextData*>(data->ptr)->base.position;
		case IMAGE:
			return static_cast<ImageData*>(data->ptr)->base.position;
		case VIEW:
			return static_cast<ViewData*>(data->ptr)->base.position;
		case BUTTON:
			return static_cast<ButtonData*>(data->ptr)->base.position;
		default:
			throw std::runtime_error("Coudn't access position of the passed in data");
		}
	}

	Vec2& UI::GetSize(UIData* data) {
		switch (data->type) {
		case TEXT:
			return static_cast<TextData*>(data->ptr)->base.size;
		case IMAGE:
			return static_cast<ImageData*>(data->ptr)->base.size;
		case VIEW:
			return static_cast<ViewData*>(data->ptr)->base.size;
		case BUTTON:
			return static_cast<ButtonData*>(data->ptr)->base.size;
		default:
			throw std::runtime_error("Coudn't access size of the passed in data");
		}
	}

	//Text

	std::string& UI::GetText(UIData* data) {
		switch (data->type) {
		case TEXT:
			return static_cast<TextData*>(data->ptr)->text;
			break;
		default:
			throw std::runtime_error("Coudn't access text of the passed in data");
		}
	}

	//Image

	Color& UI::GetColor(UIData* data) {
		switch (data->type) {
		case IMAGE:
			return static_cast<ImageData*>(data->ptr)->base.color;
		case VIEW:
			return static_cast<ViewData*>(data->ptr)->base.color;
		case BUTTON:
			return static_cast<ButtonData*>(data->ptr)->base.color;
		case TEXT:
			return static_cast<TextData*>(data->ptr)->base.color;
		default:
			throw std::runtime_error("Coudn't access color of the passed in data");
		}
	}

	int& UI::GetTexture(UIData* data) {
		switch (data->type) {
		case IMAGE:
			return static_cast<ImageData*>(data->ptr)->base.textureId;
		case VIEW:
			return static_cast<ViewData*>(data->ptr)->base.textureId;
		case BUTTON:
			return static_cast<ButtonData*>(data->ptr)->base.textureId;
		case TEXT:
			return static_cast<TextData*>(data->ptr)->base.textureId;
		default:
			throw std::runtime_error("Coudn't access texture of the passed in data");
		}
	}

	//View

	std::vector<UI::Element>* UI::GetChildrens(UIData* data) {
		switch (data->type) {
		case VIEW:
			return &static_cast<ViewData*>(data->ptr)->elements;
		default:
			throw std::runtime_error("Coudn't access childrends of the passed in data");
		}
	}

	//Button

	void*& UI::GetFunction(UIData* data) {
		switch (data->type) {
		case BUTTON:
			return static_cast<ButtonData*>(data->ptr)->function;
		default:
			throw std::runtime_error("Coudn't access function of the passed in data");
		}
	}

	UI::Text& UI::GetTextElement(UIData* data) {
		switch (data->type) {
		case BUTTON:
			return static_cast<ButtonData*>(data->ptr)->text;
		default:
			throw std::runtime_error("Coudn't access Text of the passed in data");
		}
	}

	//Variables

	int UI::mainSurface = -1;
	Render* UI::renderInstance = nullptr;
    std::unordered_map<int, Font::Font*> UI::fonts;
	int UI::nextId = 0;
    int UI::nextFontId = 0;
	std::unordered_map<int, std::vector<UI::Element>> UI::elements;
	std::unordered_set<UI::UIData*> UI::dataPtrs;
}

