#include "Ignis.h"
#include "IgnisInternal.h"
#include "tinycthread/tinycthread.h"

typedef struct IgnisArgs {
    VkInstance* instance;
    VkSurfaceKHR* surface;
    GLFWwindow* windowIn;
} IgnisArgs;

int runIgnis(void* arg) {
    IgnisArgs* args = (IgnisArgs*)arg;
    IgnisSetupInternal(args->instance, args->surface, args->windowIn);
    MainLoop();
    return 0;
}

void IgnisSetup(VkInstance* instance, VkSurfaceKHR* surface, GLFWwindow* windowIn) {
    static IgnisArgs args;
    args.instance = instance;
    args.surface = surface;
    args.windowIn = windowIn;

    thrd_t IgnisThread;
    thrd_create(&IgnisThread, runIgnis, &args);
}