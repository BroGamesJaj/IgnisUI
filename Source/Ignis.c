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
}