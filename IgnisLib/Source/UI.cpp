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

	//Other

	void UI::Bind(Element& dst, Element& src) {
		if (dst.data->type != src.data->type)
			throw std::runtime_error("Tried to bind together two different type of element");

		DeleteData(dst.data);
		dst.data = src.data;

		switch (src.data->type)
		{
		case TEXT:
			Text& textDst = static_cast<Text&>(dst);
			textDst.position = GetPosition(dst.data);
			textDst.size = GetSize(dst.data);
			textDst.text = GetText(dst.data);
			break;
		case BUTTON:
			Button& btnDst = static_cast<Button&>(dst);
			btnDst.position = GetPosition(dst.data);
			btnDst.size = GetSize(dst.data);
			btnDst.color = GetColor(dst.data);
			btnDst.textureId = GetTexture(dst.data);
			btnDst.function = GetFunction(dst.data);
			Bind(btnDst.text, GetTextElement(dst.data));
			break;
		case IMAGE:
			Image& imgDst = static_cast<Image&>(dst);
			imgDst.position = GetPosition(dst.data);
			imgDst.size = GetSize(dst.data);
			imgDst.color = GetColor(dst.data);
			imgDst.textureId = GetTexture(dst.data);
			break;
		case VIEW:
			View& viewDst = static_cast<View&>(dst);
			viewDst.position = GetPosition(dst.data);
			viewDst.size = GetSize(dst.data);
			viewDst.color = GetColor(dst.data);
			viewDst.textureId = GetTexture(dst.data);
			viewDst.elements = GetChildrens(dst.data);
			break;
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

