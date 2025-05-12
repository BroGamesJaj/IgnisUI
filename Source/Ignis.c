#include "Ignis.h"
#include "IgnisInternal.h"
#include <threads.h>

typedef struct {
    VkInstance* instance;
    VkSurfaceKHR* surface;
    GLFWwindow* windowIn;
} IgnisArgs;
int runIgnis(void* arg) {
    IgnisArgs* args = (IgnisArgs*)arg;
    printf("Thread started with args: %p\n", args);  // Debug: Make sure args is valid
    IgnisSetupInternal(args->instance, args->surface, args->windowIn);
    MainLoop();
    printf("Thread finished\n"); 
    free(args);
    return 0;
}
void IgnisSetup(VkInstance* instance, VkSurfaceKHR* surface, GLFWwindow* windowIn) {
    /*
    IgnisArgs* args = malloc(sizeof(IgnisArgs));
    if (!args) {
        printf("Failed to allocate memory for args!\n");
        return;
    }
    args->instance = instance;
    args->surface = surface;
    args->windowIn = windowIn;

    thrd_t t;
    if(thrd_create(&t, runIgnis, args)){
        printf("Failed to create thread!\n");
        free(args);
        return;
    }
    */

    IgnisSetupInternal(instance, surface, windowIn);
    MainLoop();
    IgnisShutdownInternal();
}

typedef struct Element_C {
    UIElementType type;
    void *ptr;
} Element_C;

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

int currentElementId = 0;

#ifdef __cplusplus

extern "C" {
#include "khash.h"
}

KHASH_MAP_INIT_INT(cPointers, UIElement_C*)
khash_t(cPointers)* cache = NULL;


UIElement_C* UIElement::ToC(Element_C parent) const 
{
    khiter_t k = kh_get(cPointers, cache, id);
    if (k != kh_end(cache)) {
        return kh_val(cache, k);
    }

    UIElement_C* element = new UIElement_C;
    element->id = id;
    element->uid = uid;
    element->position = position.ToC();
    element->size = size.ToC();
    element->father = parent;

    int ret;
    k = kh_put(cPointers, cache, id, &ret);
    kh_val(cache, k) = element;

    return element;
}


View::View(IgVec2 pos, IgVec2 size, ViewMode viewMode, Relatives relative = IGNIS_RELATIVE_NONE) 
{
    this->id = currentElementId++;
    this->type = IGNIS_TYPE_VIEW;
    this->position = pos;
    this->size = size;
    this->viewMode = viewMode;
    this->relative = relative;
}
InsertionProxy View::operator[](size_t index)
{
    return InsertionProxy(*this, index);
}
View& View::operator<<(const UIElement el) 
{
    elements.push_back(el);
    return *this;
}
View_C* View::ToC(Element_C parent) const 
{
    khiter_t k = kh_get(cPointers, cache, id);
    if (k != kh_end(cache)) {
        return (View_C*)kh_val(cache, k);
    }

    View_C* view = new View_C;
    view->base.id = id;
    view->base.uid = uid;
    view->base.position = position.ToC();
    view->base.size = size.ToC();
    view->viewMode = viewMode;
    view->relative = relative;
    view->base.father = parent;

    int ret;
    k = kh_put(cPointers, cache, id, &ret);
    kh_val(cache, k) = (UIElement_C*)view;

    IRat(&view->elements, 1, sizeof(Element_C));
    for (size_t i = 0; i < elements.size(); i++)
    {
        Element_C element;
        int elementId = elements[i].id;
        khiter_t k = kh_get(cPointers, cache, elementId);
        if (k != kh_end(cache)) {
            element.type = elements[i].type;
            element.ptr = kh_val(cache, k);
        }
        else {
            Element_C parent;
            parent.ptr = view;
            parent.type = type;

            if(elements[i].type == IGNIS_TYPE_UIELEMENT){
                element.type = IGNIS_TYPE_UIELEMENT;
                element.ptr = elements[i].ToC(parent);
            }
            else if(elements[i].type == IGNIS_TYPE_VIEW){
                element.type = IGNIS_TYPE_VIEW;
                element.ptr = (*(View*)&elements[i]).ToC(parent);
            }
        }

        IRatAdd(&element, &view->elements);
    }

    return view;  
}

MainView::MainView(ViewMode viewMode, Relatives relative = IGNIS_RELATIVE_NONE) 
{
    this->id = currentElementId++;
    this->type = IGNIS_TYPE_MAINVIEW;
    this->position = IgVec2(0,0);
    this->size = IgVec2(1920,1080);
    this->viewMode = viewMode;
    this->relative = relative;
}
MainView_C* MainView::ToC() const
{
    if (cache) {
        kh_destroy(cPointers, cache);
        cache = NULL;
    }
    cache = kh_init(cPointers);

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

    int ret;
    khiter_t k = kh_put(cPointers, cache, id, &ret);
    kh_val(cache, k) = (UIElement_C*)view;

    IRat(&view->base.elements, 1, sizeof(Element_C));
    for (size_t i = 0; i < elements.size(); i++)
    {
        Element_C element;
        int elementId = elements[i].id;
        khiter_t k = kh_get(cPointers, cache, elementId);
        if (k != kh_end(cache)) {
            element.ptr = kh_val(cache, k);
            element.type = elements[i].type;
        }
        else {
            Element_C parent;
            parent.ptr = view;
            parent.type = type;

            if(elements[i].type == IGNIS_TYPE_UIELEMENT){
                element.type = IGNIS_TYPE_UIELEMENT;
                element.ptr = elements[i].ToC(parent);
            }
            else if(elements[i].type == IGNIS_TYPE_VIEW){
                element.type = IGNIS_TYPE_VIEW;
                element.ptr = (*(View*)&elements[i]).ToC(parent);
            }
        }

        IRatAdd(&element, &view->base.elements);
    }

    return view; 
}

#endif

MainView_C* root;

void PrintData(){
    printf("Root id: %d", root->base.base.id);
    for (size_t i = 0; i < root->base.elements.Size; i++)
    {
        Element_C current;
        IRatGet(&current, &root->base.elements, i);
        if(current.type == IGNIS_TYPE_VIEW){
            View_C view = *(View_C*)&current;
            printf("Element type: %d, id: %d", current.type, view.base.id);
        }

    }
    
}

void SetMainView(MainView_C* mainView)
{
    root = mainView;
    PrintData();
}

