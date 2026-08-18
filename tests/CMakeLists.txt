# 使用本地已下载的 Google Test（通过 FetchContent 下载失败时使用）
set(gtest_dir "${CMAKE_SOURCE_DIR}/../googletest")
if(EXISTS "${gtest_dir}/CMakeLists.txt")
    add_subdirectory("${gtest_dir}" "${CMAKE_BINARY_DIR}/googletest" EXCLUDE_FROM_ALL)
    include(GoogleTest)
else()
    # 如果本地没有，尝试 FetchContent 下载
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://gitee.com/mirrors/googletest.git
        GIT_TAG        v1.15.2
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    include(GoogleTest)
endif()

# ---- 如果已安装 GTest，也可改用 find_package ----
# find_package(GTest REQUIRED)
# include(GoogleTest)

# 词法分析器测试
add_executable(test_lexer test_lexer.cpp)
target_link_libraries(test_lexer PRIVATE GTest::gtest_main mycompiler_lib)
target_include_directories(test_lexer PRIVATE ../src)
gtest_discover_tests(test_lexer)

# 语法分析器测试
add_executable(test_parser test_parser.cpp)
target_link_libraries(test_parser PRIVATE GTest::gtest_main mycompiler_lib)
target_include_directories(test_parser PRIVATE ../src)
gtest_discover_tests(test_parser)

# AST 节点测试
add_executable(test_ast test_ast.cpp)
target_link_libraries(test_ast PRIVATE GTest::gtest_main mycompiler_lib)
target_include_directories(test_ast PRIVATE ../src)
gtest_discover_tests(test_ast)

# 符号表测试
add_executable(test_symboltable test_symboltable.cpp)
target_link_libraries(test_symboltable PRIVATE GTest::gtest_main mycompiler_lib)
target_include_directories(test_symboltable PRIVATE ../src)
gtest_discover_tests(test_symboltable)

# 语义分析测试
add_executable(test_semantic test_semantic.cpp)
target_link_libraries(test_semantic PRIVATE GTest::gtest_main mycompiler_lib)
target_include_directories(test_semantic PRIVATE ../src)
gtest_discover_tests(test_semantic)

# IR 生成测试
add_executable(test_ir test_ir.cpp)
target_link_libraries(test_ir PRIVATE GTest::gtest_main mycompiler_lib)
target_include_directories(test_ir PRIVATE ../src)
gtest_discover_tests(test_ir)

# 优化器测试
add_executable(test_optimizer test_optimizer.cpp)
target_link_libraries(test_optimizer PRIVATE GTest::gtest_main mycompiler_lib)
target_include_directories(test_optimizer PRIVATE ../src)
gtest_discover_tests(test_optimizer)

# 代码生成测试
add_executable(test_codegen test_codegen.cpp)
target_link_libraries(test_codegen PRIVATE GTest::gtest_main mycompiler_lib)
target_include_directories(test_codegen PRIVATE ../src)
gtest_discover_tests(test_codegen)

# 代码生成测试（Google Test 版本）
add_executable(test_codegen_gtest test_codegen_gtest.cpp)
target_link_libraries(test_codegen_gtest PRIVATE GTest::gtest_main mycompiler_lib)
target_include_directories(test_codegen_gtest PRIVATE ../src)
gtest_discover_tests(test_codegen_gtest)

# 串联（端到端）测试
add_executable(test_main test_main.cpp)
target_link_libraries(test_main PRIVATE GTest::gtest_main mycompiler_lib)
target_include_directories(test_main PRIVATE ../src)
gtest_discover_tests(test_main)
