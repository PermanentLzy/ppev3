cmake_minimum_required(VERSION 3.15)
project(MyCompiler CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 收集 src/ 下所有 .cpp 文件（排除 main.cpp，它单独用于可执行文件）
file(GLOB_RECURSE LIB_SOURCES "src/*.cpp")
list(FILTER LIB_SOURCES EXCLUDE REGEX "main\\.cpp$")
file(GLOB_RECURSE MAIN_SOURCE "src/main.cpp")

# 编译库（供测试链接使用）
add_library(mycompiler_lib STATIC ${LIB_SOURCES})
target_include_directories(mycompiler_lib PUBLIC src/)

# 生成可执行文件
add_executable(mycompiler ${MAIN_SOURCE})
target_link_libraries(mycompiler PRIVATE mycompiler_lib)

# 开启测试（需要 Google Test）
enable_testing()
add_subdirectory(tests)
