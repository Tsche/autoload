#include <autoload.hpp>
#include <memory>

struct TestInterface {
  float* pi;
  void** vptr;

  void (*print)(char const* str);
};

struct UnsafeInterface {
  struct Point {
    int x;
    int y;
  };

  Point* (*make_point)(int a, int b);
  void (*destroy_point)(Point* point);
};

struct SafeLib : private erl::Library<UnsafeInterface> {
explicit SafeLib(std::string_view path) : erl::Library<UnsafeInterface>(path) {}
[[nodiscard]] auto make_point(int a, int b) const
-> std::unique_ptr<UnsafeInterface::Point, void(*)(UnsafeInterface::Point*)> {
  return {(*this)->make_point(a, b), (*this)->destroy_point};
}
};

#include <print>
int main(){
  auto test = erl::Library<TestInterface>("libtestlib.so");

  std::println("pi: {}", *test->pi);
  std::println("vptr: {}", *test->vptr);

  test->print("foo\n");

  auto wrapped = SafeLib("libunsafe.so");
  auto point = wrapped.make_point(24, 40);;
  auto [x, y] = *point;
  std::println("Point{{.x={}, .y={}}}", x, y);
}