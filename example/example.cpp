#include <autoload.h>

struct TestLib {
  float* pi;
  void** vptr;

  void (*print)(char const* str);

  struct Point {
    int x;
    int y;
  };

  Point (*foo)(int a, int b);
};

#include <print>
int main(){
  auto test = erl::Library<TestLib>("libtestlib.so");

  std::println("pi: {}", *test->pi);
  std::println("vptr: {}", *test->vptr);

  test->print("foo");

  auto [x, y] = test->foo(24, 40);
  std::println("Point{{.x={}, .y={}}}", x, y);
}