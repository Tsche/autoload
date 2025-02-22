#include <cstdio>

#if defined(_WIN32) || defined(_WIN64)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

extern "C" {

EXPORT float pi = 3.14;
EXPORT void* vptr = (void*)1234;

EXPORT void print(char const* str) {
  puts(str);
}

struct Point {
  int x;
  int y;
};

EXPORT Point foo(int a, int b) {
  return {.x=a*2, .y=b+2};
}
}