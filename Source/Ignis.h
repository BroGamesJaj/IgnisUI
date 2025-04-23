#pragma once

#include <stdio.h>

static void Hello(); 

#ifdef __cplusplus
class Ignis
{
public:
    
    static void Hello() {
        ::Hello();
    }
};
#endif