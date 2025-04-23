#include "IgnisInternal.h"

/////////Rat vector//////////
int IRat(Rat* vector, size_t capacity, size_t typeSize)
{
    if(!vector) return 1;

    vector->aSize = 0;
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

    if(vector->aSize <= vector->capacity/2) {
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
    if(index >= vector->aSize) vector->aSize = index;

    return 0;
}
int IRatAdd(void* value, Rat* vector)
{
    if(!vector) return 1;

    if(vector->aSize >= vector->capacity) IRatAlloc(vector, vector->capacity);
    
    IRatSet(value, vector, vector->aSize);
    vector->aSize++;

    return 0;
}
int IRatRemove(size_t index, Rat* vector)
{
    if(!vector) return 1;
    if(index >= vector->capacity) return 1;

    for (size_t i = index; i < vector->aSize - 1; i++) {
        memcpy((char*)vector->data + i * vector->dSize,(char*)vector->data + (i + 1) * vector->dSize, vector->dSize);
    }

    memset((char*)vector->data + (vector->aSize - 1) * vector->dSize, 0, vector->dSize);
    vector->aSize--;

    return IRatResize(vector);
}
int IRatPopLast(Rat* vector)
{
    if(!vector) return 1;
    if(!vector->aSize == 0) return 1;

    vector->aSize--;

    return IRatResize(vector);
}
int IRatPopFirst(Rat* vector)
{
    if(!vector) return 1;
    if(!vector->aSize == 0) return 1;

    for (size_t i = 0; i < vector->aSize - 1; i++) {
        memcpy((char*)vector->data + i * vector->dSize,(char*)vector->data + (i + 1) * vector->dSize, vector->dSize);
    }

    vector->aSize--;

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
/////////////////////////////

