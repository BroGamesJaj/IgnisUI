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



int currentElementId = 0;

MainView_C* root;

void PrintData(){
    printf("Root size: %zd\n", root->base.elements.Size);
    for (size_t i = 0; i < root->base.elements.Size; i++)
    {
        Element_C current;
        IRatGet(&current, &root->base.elements, i);
        if(current.type == IGNIS_TYPE_VIEW){
            View_C view = *(View_C*)current.ptr;
            printf("Element type: %d, id: %d\n", current.type, view.base.id);
        }
    }
    
}

void SetMainView(MainView_C* mainView)
{
    root = mainView;
    PrintData();
}
