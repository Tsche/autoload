project(erl C CXX)

add_library(testlib SHARED "lib/testlib.c")
add_library(unsafe SHARED "lib/unsafe.c")

add_executable(example "example.cpp")
target_link_libraries(example PRIVATE autoload)
