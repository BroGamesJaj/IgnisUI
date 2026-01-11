#include "IgnisLib.h"

using Vec2 = Ignis::UI::Vec2;
using Vertex = Ignis::Render::Vertex;
using Color = Ignis::UI::Color;

namespace Ignis {

	//Data Handling

	UI::UIData::UIData(void* ptr, UIType type) : ptr(ptr), type(type) {}

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
		if (!data) return;

		switch (data->type)
		{
		case TEXT:
			delete static_cast<TextData*>(data->ptr);
			break;
		default:
			throw std::runtime_error("Coudn't clean up UIData pointer");
		}

		delete data;
		data = nullptr;
	}

	//Element

	Vec2& UI::GetPosition(UIData* data) {
		switch (data->type) {
		case TEXT:
			return static_cast<TextData*>(data->ptr)->base.position;
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
			return static_cast<ImageData*>(data->ptr)->color;
		case VIEW:
			return static_cast<ViewData*>(data->ptr)->color;
		case BUTTON:
			return static_cast<ButtonData*>(data->ptr)->color;
		default:
			throw std::runtime_error("Coudn't access color of the passed in data");
		}
	}

	int& UI::GetTexture(UIData* data) {
		switch (data->type) {
		case IMAGE:
			return static_cast<ImageData*>(data->ptr)->textureId;
		case VIEW:
			return static_cast<ViewData*>(data->ptr)->textureId;
		case BUTTON:
			return static_cast<ButtonData*>(data->ptr)->textureId;
		default:
			throw std::runtime_error("Coudn't access texture of the passed in data");
		}
	}

	//View

	std::vector<UI::Element>& UI::GetChildrens(UIData* data) {
		switch (data->type) {
		case VIEW:
			return static_cast<ViewData*>(data->ptr)->elements;
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

	void UI::AddToSurface(Element& element, int surface) {
		if (!elements.contains(surface)) return;
		elements[surface].push_back(element);
	}
	void UI::SubmitSurface(int surface) {
		if (!elements.contains(surface)) return;

		//UI::ProcessData data{.offset{0,0},.size{100,100}};

		//ProcessVertecies(data, elements[surface]);
	}
	UI::ProcessData UI::ProcessVertecies(UI::ProcessData data, std::vector<Element>& elements) {

		for (auto& element : elements) {
		}

		return data;
	}

	int UI::mainSurface = -1;
	Render* UI::renderInstance = nullptr;
	int UI::nextId = 0;
}

