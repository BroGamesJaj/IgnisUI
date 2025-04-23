#include "IgnisInternal.h"

int IRat(Rat* vector, size_t capacity, size_t typeSize)
{
    if(!vector) return 1;

    vector->aSize = 0;
    vector->capacity = capacity;
    vector->dSize = typeSize;
    vector->data = (void*)malloc(capacity * typeSize);

    return 0;
}