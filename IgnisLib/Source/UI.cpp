#include "IgnisLib.h"

using Vec2 = Ignis::UI::Vec2;
using Vertex = Ignis::Render::Vertex;

namespace Ignis {

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

