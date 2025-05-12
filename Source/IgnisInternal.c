#include "IgnisInternal.h"

/////////Rat vector//////////
int IRat(Rat* vector, size_t capacity, size_t typeSize)
{
    if(!vector) return 1;

    vector->Size = 0;
    vector->capacity = capacity;
    vector->dSize = typeSize;
    vector->data = (void*)malloc(capacity * typeSize);

    return 0;
}
int IRatAlloc(Rat* vector, size_t allocation)
{
    if(!vector) return 1;

    void* newData = realloc(vector->data, (vector->capacity + allocation) * vector->dSize);
    if(!newData) return 1;

    vector->capacity = vector->capacity + allocation;
    vector->data = newData;

    return 0;
}
int IRatResize(Rat* vector){
    if(!vector) return 1;

    if(vector->Size <= vector->capacity/2) {
        size_t newCapacity = vector->capacity / 2;
        if (newCapacity < 1) newCapacity = 1;

        void* newData = realloc(vector->data, (vector->capacity/2) * vector->dSize);
        if(!newData) return 1;

        vector->data = newData;
        vector->capacity = newCapacity;
    }

    return 0;
}
int IRatGet(void* value, Rat* vector, size_t index)
{
    if(!vector) return 1;

    if(index >= vector->capacity) return 1;

    memcpy(value, (char*)vector->data + index * vector->dSize, vector->dSize);

    return 0;
}
int IRatSet(void* value, Rat* vector, size_t index)
{
    if(!vector) return 1;
    if(index >= vector->capacity) return 1;

    memcpy((char*)vector->data + index * vector->dSize, value, vector->dSize);
    if(index >= vector->Size) vector->Size = index;

    return 0;
}
int IRatAdd(void* value, Rat* vector)
{
    if(!vector) return 1;

    if(vector->Size >= vector->capacity) IRatAlloc(vector, vector->capacity);
    
    IRatSet(value, vector, vector->Size);
    vector->Size++;

    return 0;
}
int IRatRemove(size_t index, Rat* vector)
{
    if(!vector) return 1;
    if(index >= vector->capacity) return 1;

    for (size_t i = index; i < vector->Size - 1; i++) {
        memcpy((char*)vector->data + i * vector->dSize,(char*)vector->data + (i + 1) * vector->dSize, vector->dSize);
    }

    memset((char*)vector->data + (vector->Size - 1) * vector->dSize, 0, vector->dSize);
    vector->Size--;

    return IRatResize(vector);
}
int IRatPopLast(Rat* vector)
{
    if(!vector) return 1;
    if(!vector->Size == 0) return 1;

    vector->Size--;

    return IRatResize(vector);
}
int IRatPopFirst(Rat* vector)
{
    if(!vector) return 1;
    if(!vector->Size == 0) return 1;

    for (size_t i = 0; i < vector->Size - 1; i++) {
        memcpy((char*)vector->data + i * vector->dSize,(char*)vector->data + (i + 1) * vector->dSize, vector->dSize);
    }

    vector->Size--;

    return IRatResize(vector);
}
int IRatFree(Rat* vector)
{
    if(!vector) return 1;
    if(!vector->data) return 1;

    free(vector->data);
    vector->data = NULL;

    return 0;
}
int IRatCheckSize(Rat* vector)
{
    if (!vector || !vector->data) return 1;

    vector->Size = 0;

    for (size_t i = 0; i < vector->capacity; i++)
    {
        void* itemPtr = (char*)vector->data + i * vector->dSize;
        bool isNonZero = false;

        for (size_t j = 0; j < vector->dSize; j++)
        {
            if (*((unsigned char*)itemPtr + j) != 0)
            {
                isNonZero = true;
                break;
            }
        }
        if (isNonZero) vector->Size++;
    }

    return 0;
}
int IRatEmpty(bool* isEmpty, Rat* vector)
{
    if(!vector) return 1;

    *isEmpty = true;

    for (size_t i = 0; i < vector->capacity; i++)
    {
        void* itemPtr = (char*)vector->data + i * vector->dSize;

        for (size_t j = 0; j < vector->dSize; j++)
        {
            if(*((unsigned char*)itemPtr+j) != 0)
            {
                *isEmpty = false;
            }
        }  
    }

    return 0;
}
/////////////////////////////

VkVertexInputBindingDescription GetBindingDescription() 
{
    VkVertexInputBindingDescription bindingDescription = {0};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}
void GetAttributeDescriptions(VkVertexInputAttributeDescription* out) 
{
    out[0].binding = 0;
    out[0].location = 0;
    out[0].format = VK_FORMAT_R32G32_SFLOAT;
    out[0].offset = offsetof(Vertex, pos);

    out[1].binding = 0;
    out[1].location = 1;
    out[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    out[1].offset = offsetof(Vertex, color);

    out[2].binding = 0;
    out[2].location = 2;
    out[2].format = VK_FORMAT_R32G32_SFLOAT;
    out[2].offset = offsetof(Vertex, uv);

    out[3].binding = 0;
    out[3].location = 3;
    out[3].format = VK_FORMAT_R32_SINT;
    out[3].offset = offsetof(Vertex, textureIndex);
}

size_t ReadFile(const char* filename, char** buffer) 
{
    FILE* file = fopen(filename, "rb");
    if (!file) return 0;

    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    *buffer = (char*)malloc(fileSize);
    if (!*buffer) {
        fclose(file);
        return 0;
    }

    fread(*buffer, 1, fileSize, file);
    fclose(file);

    return fileSize;
}

