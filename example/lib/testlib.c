#include <stdio.h>


#if defined(_WIN32) || defined(_WIN64)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

EXPORT float pi = 3.14;
EXPORT void* vptr = (void*)1234;

EXPORT void print(char const* str) {
  puts(str);
}