#pragma once

#include <stdio.h>

#ifdef __cplusplus
namespace ignis_internal 
{
	extern "C" {
#endif
		void Hello();
#ifdef __cplusplus
	}
}
#endif

#ifdef __cplusplus
class Ignis {
public:
	static void Hello() {
		ignis_internal::Hello();
	}
};
#endif