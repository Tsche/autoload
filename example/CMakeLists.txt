project(erl)

add_library(testlib SHARED "library.cpp")

add_executable(example "example.cpp")
target_link_libraries(example PRIVATE autoload)
