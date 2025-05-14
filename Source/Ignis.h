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

typedef struct Rat Rat;

typedef struct Element_C{
	UIElementType type;
	void* ptr;
} Element_C;
typedef struct IgVec2_C {
    int x, y;
} IgVec2_C;
typedef struct UIElement_C {
    int id;
    char* uid; 
    IgVec2_C position;
    IgVec2_C size;
    Element_C father;
} UIElement_C;
typedef struct View_C {
    UIElement_C base;
    ViewMode viewMode;
    Relatives relative;
    Rat/*Element_C*/ elements; 
} View_C;
typedef struct MainView_C {
    View_C base;
} MainView_C;

#ifdef __cplusplus
struct IgVec2 {
    int x, y;
    IgVec2() = default;
    IgVec2(int xIn, int yIn) : x(xIn), y(yIn) {}

    IgVec2_C ToC() const {
        IgVec2_C out;
        out.x = x;
        out.y = y;
        return out;
    }
};
#endif

#ifdef __cplusplus
class UIElement {
public:
	UIElementType type = IGNIS_TYPE_UIELEMENT;
    int id = -1;
    char* uid; 
    IgVec2 position;
    IgVec2 size;
    UIElement* father = NULL;

    UIElement_C* ToC(Element_C parent) const {
    UIElement_C* element = new UIElement_C;
    element->id = id;
    element->uid = uid;
    element->position = position.ToC();
    element->size = size.ToC();
    element->father = parent;

    return element;
    }
};

//////// Views ////////
struct InsertionProxy;
class View : public UIElement {
protected:
    ViewMode viewMode = IGNIS_VIEW_CONTINOUS;
    Relatives relative = IGNIS_RELATIVE_VIEW;
public:
    std::vector<Element_C> vElements;
	int elementCount = 0;

    View() = default;
    View(IgVec2 pos, IgVec2 size, ViewMode viewMode, Relatives relative = IGNIS_RELATIVE_NONE) {
        this->id = currentElementId++;
        printf("Id: %d\n", currentElementId);
        this->type = IGNIS_TYPE_VIEW;
        this->position = pos;
        this->size = size;
        this->viewMode = viewMode;
        this->relative = relative;
        this->vElements.clear();
    }
    InsertionProxy operator[](size_t index) {
        return InsertionProxy(*this, index);
    }
    View& operator<<(const UIElement* el) {
        Element_C toVt;
        toVt.type = (*el).type;
        if(toVt.type == IGNIS_TYPE_VIEW){
            toVt.ptr = new View(*(View*)el);
        }
        vElements.push_back(toVt);
        elementCount++;
        return *this;
    }
	View_C* ToC(Element_C parent) const {
        View_C* view = new View_C;
        view->base.id = id;
        view->base.uid = uid;
        view->base.position = position.ToC();
        view->base.size = size.ToC();
        view->viewMode = viewMode;
        view->relative = relative;
        view->base.father = parent;

        IRat(&view->elements, 1, sizeof(Element_C));
        for (size_t i = 0; i < elementCount; i++)
        {
            Element_C element;
            
            Element_C parentTo;
            parentTo.ptr = view;
            parentTo.type = type;

            if(vElements[i].type == IGNIS_TYPE_UIELEMENT){
                element.type = IGNIS_TYPE_UIELEMENT;
                element.ptr = (*(UIElement*)vElements[i].ptr).ToC(parentTo);
            }
            else if(vElements[i].type == IGNIS_TYPE_VIEW){
                element.type = IGNIS_TYPE_VIEW;
                element.ptr = (*(View*)vElements[i].ptr).ToC(parentTo);
            }
        

            IRatAdd(&element, &view->elements);
        }
        return view;  
    }
};
struct InsertionProxy {
    View& view;
    int index;

    InsertionProxy(View& v, int i) : view(v), index(i) {}

    void operator--(int)
    {
        view.vElements.erase(view.vElements.begin()+index);
    }

    InsertionProxy& operator<<(const UIElement* el){
        if (index >= view.vElements.size()) {
            view.vElements.resize(index + 1);
        }
		Element toVt;
		toVt.type = (*el).type;
		if(toVt.type == IGNIS_TYPE_VIEW){
			toVt.ptr = new View;
			*((View*)toVt.ptr) = *(View*)el;
		}
        view.vElements.insert(view.vElements.begin() + index + 1, toVt);
        index++;
		view.elementCount++;
        return *this;
    }
};
class MainView : public View {
public:
    MainView(ViewMode viewMode, Relatives relative = IGNIS_RELATIVE_NONE) {
        this->id = currentElementId++;
        this->type = IGNIS_TYPE_MAINVIEW;
        this->position = IgVec2(0,0);
        this->size = IgVec2(1920,1080);
        this->viewMode = viewMode;
        this->relative = relative;
    }
	MainView_C* ToC() const {
        MainView_C* view = new MainView_C;
        view->base.base.id = id;
        view->base.base.uid = uid;
        IgVec2_C pos, si; // si = windowSize
        pos.x = 0;
        pos.y = 0; 
        si.x = 1920;
        si.y = 1080;
        view->base.base.position = pos;
        view->base.base.size = si;
        view->base.viewMode = viewMode;
        view->base.relative = relative;
        view->base.base.father.ptr = NULL;

        IRat(&view->base.elements, 1, sizeof(Element_C));
        for (size_t i = 0; i < vElements.size(); i++)
        {
            Element_C element;
            Element_C parent;
            parent.ptr = view;
            parent.type = type;

            if(vElements[i].type == IGNIS_TYPE_UIELEMENT){
                    element.type = IGNIS_TYPE_UIELEMENT;
                    element.ptr = (*(UIElement*)vElements[i].ptr).ToC(parent);
                }
            else if(vElements[i].type == IGNIS_TYPE_VIEW){
                    element.type = IGNIS_TYPE_VIEW;
                    element.ptr = (*(View*)vElements[i].ptr).ToC(parent);
            }

            IRatAdd(&element, &view->base.elements);
        }

        return view; 
    }
};
///////////////////////
#endif


#ifdef __cplusplus
namespace ignis_internal{
	extern "C" {
#endif

	void IgnisSetup(VkInstance* instance, VkSurfaceKHR* surface, GLFWwindow* windowIn);
	void SetMainView(MainView_C* mainView);
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
		MainView_C* mainView_C = mainView.ToC();
		ignis_internal::SetMainView(mainView_C);
    }
	static void LoadView(){
		ignis_internal::LoadView();
	}
};
#endif