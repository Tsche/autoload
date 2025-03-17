#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#if defined(_WIN32) || defined(_WIN64)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

typedef struct {
  int x;
  int y;
} Point;

EXPORT Point* make_point(int a, int b) {
  Point* point = malloc(sizeof(Point));
  assert(point);
  point->x = a*2;
  point->y = b+2;
  return point;
}

EXPORT void destroy_point(Point* point){
  free(point);
}
