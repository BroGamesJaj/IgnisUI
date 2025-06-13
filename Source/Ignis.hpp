#pragma once

#define GLFW_INCLUDE_NONE

#include "vulkan/vulkan.h" // Vulkan header
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"


#ifdef __cplusplus
#include <vector>
#else 
#define IgVec2_C IgVec2
#define UIElement_C UIElement
#endif


typedef enum {
	IGNIS_TYPE_UIELEMENT,
	IGNIS_TYPE_VIEW,
	IGNIS_TYPE_MAINVIEW
} UIElementType;

typedef enum {
    IGNIS_VIEW_DISCRATE,
    IGNIS_VIEW_CONTINOUS,
} ViewMode;

typedef enum {
    IGNIS_RELATIVE_NONE,
    IGNIS_RELATIVE_SCREEN,
    IGNIS_RELATIVE_WINDOW,
    IGNIS_RELATIVE_VIEW
} Relatives;

typedef struct IgVec2 {
    int x;
    int y;
} IgVec2;

typedef struct ElementData ElementData;

#ifdef __cplusplus

////// Elements ///////
ElementData* ElementCreate();
UIElementType ElementGetType(ElementData* e);
void ElementDestroy(ElementData* e);
int ElementGetId(ElementData* e);
IgVec2* ElementGetPos(ElementData* e);
IgVec2* ElementGetSize(ElementData* e);

class UIElement {
protected:
    ElementData* data;
public:
    const int id;
    IgVec2& position;
    IgVec2& size;

    UIElement() : data(ElementCreate()), id(ElementGetId(data)),
    position(*ElementGetPos(data)), size(*ElementGetSize(data)){}

    UIElement(ElementData* inData) : data(inData), id(ElementGetId(data)),
    position(*ElementGetPos(data)), size(*ElementGetSize(data)){}

    ~UIElement()  {ElementDestroy(data);}

    ElementData* GetData() const { return data; }
};

//////// Views ////////
ElementData* ViewCreate(IgVec2 pos, IgVec2 size, ViewMode viewMode, Relatives relative);
void ViewDestroy(ElementData* e);
ViewMode ViewGetViewMode(ElementData* e);
Relatives ViewGetRelative(ElementData* e);
ElementData** ViewGetElements(ElementData* e);
int ViewGetElementsCount(ElementData* e);
UIElement* ElementFromData(ElementData* e);
void ViewAddElement(ElementData* e, ElementData* elementToAdd);
void ViewDeleteElement(ElementData* e, int index);
void ViewInsertElement(ElementData* e, ElementData* elementToAdd, int index);

ElementData* MainViewCreate(ViewMode viewMode, Relatives relative);

struct InsertionProxy;
class View : public UIElement {
public:
    std::vector<UIElement*> elements;
    const ViewMode viewMode;
    const Relatives relatives;

    View(IgVec2 pos, IgVec2 size, ViewMode viewModeIn, Relatives relativeIn = IGNIS_RELATIVE_NONE) : 
        viewMode(viewModeIn), relatives(relativeIn) {
        data = ViewCreate(pos, size, viewModeIn, relativeIn);
    }
    View(ElementData* dataIn) : viewMode(ViewGetViewMode(dataIn)), relatives(ViewGetRelative(dataIn)){
        data = dataIn;
        ElementData** childs = ViewGetElements(data);
        int childCount = ViewGetElementsCount(data);
        for (int i = 0; i < childCount; i++)
        {
            elements.push_back(ElementFromData(childs[i]));
        }
    }
    View(ViewMode viewModeIn, Relatives relativeIn = IGNIS_RELATIVE_NONE) : 
        viewMode(viewModeIn), relatives(relativeIn) {
        data = MainViewCreate(viewModeIn, relativeIn);
    }

    void AddElement(UIElement* element){
        elements.push_back(ElementFromData(element->GetData()));
        ViewAddElement(data, element->GetData());
    }
    void RemoveElement(int id){
        if(id >= 0 && id < elements.size()){
            ViewDeleteElement(data, id);
            delete elements[id];
            elements.erase(elements.begin() + id);
        }
    }
    void InsertElement(int index, UIElement* element) {
        elements.insert(elements.begin() + index + 1, element);
        ViewInsertElement(data, element->GetData(), index);
    }

    InsertionProxy operator[](size_t index) {
        return InsertionProxy(*this, index);
    }
    View& operator<<(UIElement* element) {
        elements.push_back(element);
        ViewAddElement(data, element->GetData());
        return *this;
    }
};
struct InsertionProxy {
    View& view;
    int index;

    InsertionProxy(View& v, int i) : view(v), index(i) {}

    void operator--(int)
    {
        view.RemoveElement(index);
    }

    InsertionProxy& operator<<(UIElement* el){
        view.InsertElement(index, el);
        return *this;
    }
};

void ViewDestroy(ElementData* e);

class MainView : public View {
public:
    MainView(ViewMode viewModeIn, Relatives relativeIn = IGNIS_RELATIVE_NONE) : View(viewModeIn, relativeIn) {}
};
///////////////////////
#endif


#ifdef __cplusplus
namespace ignis_internal{
	extern "C" {
#endif

	void IgnisSetup(VkInstance* instance, VkSurfaceKHR* surface, GLFWwindow* windowIn);
	void SetMainView(MainView* mainView);
	void LoadView();

#ifdef __cplusplus
	}
}
#endif


#ifdef __cplusplus
class Ignis {
public:
	static void IgnisSetup(VkInstance* instance, VkSurfaceKHR* surface, GLFWwindow* windowIn) {
		ignis_internal::IgnisSetup(instance, surface, windowIn);
	}
	static void SetMainView(MainView mainView){
		ignis_internal::SetMainView(&mainView);
    }
	static void LoadView(){
		ignis_internal::LoadView();
	}
};
#endif