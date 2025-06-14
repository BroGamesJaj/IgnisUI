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


typedef struct ElementData {
    void* ptr;
    UIElementType type;
} ElementData;

typedef struct Element {
    int id;
    IgVec2 pos;
    IgVec2 size;
    Element* father;
    char* uid;
    int uidSize;
} Element;

typedef struct View {
    Element base;
    ViewMode viewMode;
    Relatives relative;
    Rat* elements; 
} View;
typedef struct MainView {
    View base;
} MainView;

int currentElementId = 1;
MainView* root;
IgVec2 maxSizePx;
IgVec2 maxSizeRatio;
IgVec2 origin = {0};


/*
void PrintData(){
    printf("Root size: %zd\n", root->base.elements->Size);
    for (size_t i = 0; i < root->base.elements->Size; i++)
    {
        Element current;
        IRatGet(&current, root->base.elements, i);
        if(current.type == IGNIS_TYPE_VIEW){
            View view = *(View*)current.ptr;
            printf("Element type: %d, id: %d\n", current.type, view.base.id);
        }
    }
    
}
*/

UIElementType ElementGetType(ElementData* e)
{
    return e->type;
}
int ElementGetId(ElementData* e)
{
    if(e->type == IGNIS_TYPE_UIELEMENT){
        Element* data = (Element*)e->ptr;
        return data->id;
    }
}
IgVec2* ElementGetPos(ElementData* e)
{
    if(e->type == IGNIS_TYPE_UIELEMENT){
        Element* data = (Element*)e->ptr;
        return &data->pos;
    }
}
IgVec2* ElementGetSize(ElementData* e)
{
    if(e->type == IGNIS_TYPE_UIELEMENT){
        Element* data = (Element*)e->ptr;
        return &data->size;
    }
}

ElementData* ElementCreate(IgVec2 posIn, IgVec2 sizeIn)
{
    ElementData* data = malloc(sizeof(ElementData));
    data->type = IGNIS_TYPE_UIELEMENT;

    Element* element = malloc(sizeof(Element));
    element->id = currentElementId++;
    element->pos = posIn;
    element->size = sizeIn;
    element->father = NULL;
    element->uid = NULL;
    element->uidSize = 0;

    data->ptr = element;

    return data;
}
void ElementDestroy(ElementData* e)
{
    if(e->type == IGNIS_TYPE_UIELEMENT){
        Element* ptr = (Element*)e->ptr;
        free(ptr->uid); 
        free(e->ptr);
        free(e);
    }
}

ElementData* MainViewCreate(ViewMode modeIn, Relatives relativeIn)
{
    ElementData* data = malloc(sizeof(ElementData));
    data->type = IGNIS_TYPE_MAINVIEW;

    MainView* mainView = malloc(sizeof(MainView));

    Element base;
    base.id = currentElementId++;
    base.pos = origin;
    if(modeIn = IGNIS_VIEW_DISCRATE) base.size = maxSizePx;
    else base.size = maxSizeRatio;
    base.father = NULL;
    base.uid = NULL;
    base.uidSize = 0;

    View view;
    view.base = base;
    view.relative = relativeIn;
    view.viewMode = modeIn;

    Rat* array = malloc(sizeof(Rat));
    IRat(array, 1, sizeof(ElementData));
    view.elements = array;

    mainView->base = view;

    data->ptr = mainView;

    return data;
}
void MainViewDestroy(ElementData* e)
{
    if(e->type == IGNIS_TYPE_MAINVIEW){
        MainView* data = (MainView*)e->ptr;
        View view = data->base;
        Element element = view.base;
        free(element.uid);
        IRatFree(view.elements);
        free(e->ptr);
        free(e);
    }
}

ElementData* ViewCreate(IgVec2 posIn, IgVec2 sizeIn, ViewMode viewModeIn, Relatives relativeIn)
{
    ElementData* data = malloc(sizeof(ElementData));
    data->type = IGNIS_TYPE_VIEW;

    View* view = malloc(sizeof(View));
    
    Element baseData;
    baseData.id = currentElementId++;
    baseData.pos = posIn;
    baseData.size = sizeIn;
    baseData.father = NULL;
    baseData.uid = NULL;
    baseData.uidSize = 0;

    view->base = baseData;
    view->relative = relativeIn;
    view->viewMode = viewModeIn;

    Rat* array = malloc(sizeof(Rat));
    IRat(array, 1, sizeof(ElementData));
    view->elements = array;

    data->ptr = view;

    return data;
}
void ViewDestroy(ElementData* e)
{
    if(e->type == IGNIS_TYPE_VIEW){
        View* data = (View*)e->ptr;
        Element element = data->base;
        free(element.uid);
        IRatFree(data->elements);
        free(data);
        free(e);
    }
}

ViewMode ViewGetViewMode(ElementData* e)
{
    if(e->type == IGNIS_TYPE_MAINVIEW){
        MainView* data = (MainView*)e->ptr;
        return data->base.viewMode;
    }
    else if(e->type == IGNIS_TYPE_VIEW){
        View* data = (View*)e->ptr;
        return data->viewMode;
    }
}
Relatives ViewGetRelative(ElementData* e)
{
    if(e->type == IGNIS_TYPE_MAINVIEW){
        MainView* data = (MainView*)e->ptr;
        return data->base.relative;
    }
    else if(e->type == IGNIS_TYPE_VIEW){
        View* data = (View*)e->ptr;
        return data->relative;
    }
}
ElementData** ViewGetElements(ElementData* e)
{
    if(e->type == IGNIS_TYPE_MAINVIEW){
        MainView* data = (MainView*)e->ptr;
        return data->base.elements->data;
    }
    else if(e->type == IGNIS_TYPE_VIEW){
        View* data = (View*)e->ptr;
        return data->elements->data;
    }
}
int ViewGetElementsCount(ElementData* e)
{
    if(e->type == IGNIS_TYPE_MAINVIEW){
        MainView* data = (MainView*)e->ptr;
        return data->base.elements->Size;
    }
    else if(e->type == IGNIS_TYPE_VIEW){
        View* data = (View*)e->ptr;
        return data->elements->Size;
    }
}

void ViewAddElement(ElementData* e, ElementData* elementToAdd)
{
    if(e->type == IGNIS_TYPE_MAINVIEW){
        MainView* data = (MainView*)e->ptr;
        IRatAdd(elementToAdd, data->base.elements);
    }
    else if(e->type == IGNIS_TYPE_VIEW){
        View* data = (View*)e->ptr;
        IRatAdd(elementToAdd, data->elements);
    }
}
void ViewDeleteElement(ElementData* e, int index)
{
    if(e->type == IGNIS_TYPE_MAINVIEW){
        MainView* data = (MainView*)e->ptr;
        IRatRemove(index, data->base.elements);
    }
    else if(e->type == IGNIS_TYPE_VIEW){
        View* data = (View*)e->ptr;
        IRatRemove(index, data->elements);
    }
}
void ViewInsertElement(ElementData* e, ElementData* elementToAdd, int index){
    if(e->type == IGNIS_TYPE_MAINVIEW){
        MainView* data = (MainView*)e->ptr;
        IRatInsert(index, e, data->base.elements);
    }
    else if(e->type == IGNIS_TYPE_VIEW){
        View* data = (View*)e->ptr;
        IRatInsert(index, e, data->elements);
    }
}

void SetMainView(MainView* mainView)
{
    root = mainView;
    PrintData();
}
