#include "vulkan/vulkan.h" //delete this later

#include <stdio.h>
#include <stdlib.h>

#define bool char
#define true 1
#define false 0

/////////Rat vector//////////
typedef struct Rat {
    void* data;
    size_t dSize; //data type size
    size_t Size; //array size
    size_t capacity;
} Rat;

int IRat(Rat* vector, size_t capacity, size_t typeSize);
int IRatAlloc(Rat* vector, size_t allocation);
int IRatResize(Rat* vector);
int IRatGet(void* value, Rat* vector, size_t index);
int IRatSet(void* value, Rat* vector, size_t index);
int IRatAdd(void* value, Rat* vector);
int IRatRemove(size_t index, Rat* vector);
int IRatPopLast(Rat* vector);
int IRatPopFirst(Rat* vector);
int IRatFree(Rat* vector);
int IRatCheckSize(Rat* vector);
int IRatEmpty(bool* isEmpty, Rat* vector);
/////////////////////////////

///////////Vulkan////////////
typedef struct Vertex {
    float pos[2];
    float color[3];
    float uv[2];
    int textureIndex;
} Vertex;

typedef struct UniformBufferData {
    int testValue;
} UniformBufferData;

int IgnisSetupInternal(VkInstance* instance, VkSurfaceKHR* surface);
/////////////////////////////

