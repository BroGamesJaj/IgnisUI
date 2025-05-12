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

typedef struct Element_C Element_C;
typedef struct IgVec2_C {
    int x, y;
} IgVec2_C;
typedef struct UIElement_C UIElement_C;
typedef struct View_C View_C;
typedef struct MainView_C MainView_C;

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

    UIElement_C* ToC(Element_C parent) const;
};
#endif

#ifdef __cplusplus

struct InsertionProxy;

class View : public UIElement {
protected:
    ViewMode viewMode = IGNIS_VIEW_CONTINOUS;
    Relatives relative = IGNIS_RELATIVE_VIEW;
public:
    std::vector<UIElement> vElements;

    View() = default;
    View(IgVec2 pos, IgVec2 size, ViewMode viewMode, Relatives relative = IGNIS_RELATIVE_NONE);
    InsertionProxy operator[](size_t index);
    View& operator<<(const UIElement el);
	View_C* ToC(Element_C parent) const;
};

struct InsertionProxy {
    View& view;
    int index;

    InsertionProxy(View& v, int i) : view(v), index(i) {}

    void operator--(int)
    {
        view.vElements.erase(view.vElements.begin()+index);
    }

    InsertionProxy& operator<<(const UIElement el){
        if (index >= view.vElements.size()) {
            view.vElements.resize(index + 1);
        }
        view.vElements.insert(view.vElements.begin() + index + 1, el);
        index++;
        return *this;
    }
};

class MainView : public View {
public:
    MainView(ViewMode viewMode, Relatives relative = IGNIS_RELATIVE_NONE);
	MainView_C* ToC() const;
};

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