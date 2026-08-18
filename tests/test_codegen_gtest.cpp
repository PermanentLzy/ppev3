/**
 * @file test_codegen_gtest.cpp
 * @brief 代码生成器（CodeGen）测试 —— Google Test 版本
 *
 * 测试范围：
 *   1. 基础赋值与常量
 *   2. 算术运算（加减乘除取模）
 *   3. 比较运算（小于、小于等于、大于、大于等于、等于、不等于）
 *   4. 一元运算（负号、逻辑非）
 *   5. 控制流（if、if-else、while）
 *   6. 函数调用与返回
 *   7. 汇编结构
 *   8. 复杂表达式
 *   9. 边界情况
 */

#include <gtest/gtest.h>
#include "../src/lexer/Lexer.h"
#include "../src/parser/Parser.h"
#include "../src/parser/ParseError.h"
#include "../src/ir/IRGenerator.h"
#include "../src/ir/ThreeAddrCode.h"
#include "../src/codegen/CodeGen.h"

#include <sstream>
#include <string>

using namespace MyCompiler;

// ================================================================
//  辅助函数
// ================================================================

/// 源码 → AST → IR
static std::unique_ptr<TACProgram> compileToIR(const std::string &src)
{
    Lexer lexer(src);
    Parser parser(lexer);
    std::unique_ptr<Program> ast;
    try
    {
        ast = parser.parse();
    }
    catch (const ParseError &e)
    {
        ADD_FAILURE() << "Parse error: " << e.what();
        return nullptr;
    }
    IRGenerator irGen;
    return irGen.generate(*ast);
}

/// 捕获 CodeGen 输出
static std::string captureCodeGenOutput(const TACProgram &program)
{
    std::streambuf *oldCout = std::cout.rdbuf();
    std::ostringstream oss;
    std::cout.rdbuf(oss.rdbuf());

    CodeGen codeGen;
    codeGen.generate(program);

    std::cout.rdbuf(oldCout);
    return oss.str();
}

/// 源码 → 汇编
static std::string compileToAssembly(const std::string &src)
{
    auto prog = compileToIR(src);
    if (!prog)
        return "";
    return captureCodeGenOutput(*prog);
}

/// 检查汇编输出是否包含指定行
static bool containsLine(const std::string &asm_output, const std::string &line)
{
    std::istringstream iss(asm_output);
    std::string current_line;
    while (std::getline(iss, current_line))
    {
        size_t start = current_line.find_first_not_of(" \t");
        if (start != std::string::npos)
        {
            current_line = current_line.substr(start);
        }
        if (current_line == line)
            return true;
    }
    return false;
}

/// 统计汇编中指定指令的出现次数
static int countInstruction(const std::string &asm_output, const std::string &instr)
{
    int count = 0;
    std::istringstream iss(asm_output);
    std::string line;
    while (std::getline(iss, line))
    {
        size_t pos = line.find(instr);
        if (pos != std::string::npos)
        {
            if (pos == 0 || line[pos - 1] == ' ' || line[pos - 1] == '\t')
            {
                count++;
            }
        }
    }
    return count;
}

// ================================================================
//  1. 基础赋值与常量
// ================================================================

TEST(CodeGenTest, SimpleAssignment)
{
    std::string asm_out = compileToAssembly("int f() { int x = 42; }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_TRUE(containsLine(asm_out, "li t0, 42"));
    EXPECT_GE(countInstruction(asm_out, "sw"), 1);
}

TEST(CodeGenTest, VariableCopy)
{
    std::string asm_out = compileToAssembly("int f() { int a = 1; int b = a; }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_TRUE(containsLine(asm_out, "li t0, 1"));
    EXPECT_GE(countInstruction(asm_out, "sw"), 2);
}

// ================================================================
//  2. 算术运算
// ================================================================

TEST(CodeGenTest, Addition)
{
    std::string asm_out = compileToAssembly("int f() { int x = 1 + 2; }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_TRUE(containsLine(asm_out, "add t0, t0, t1"));
}

TEST(CodeGenTest, Subtraction)
{
    std::string asm_out = compileToAssembly("int f() { int x = 5 - 3; }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_TRUE(containsLine(asm_out, "sub t0, t0, t1"));
}

TEST(CodeGenTest, Multiplication)
{
    std::string asm_out = compileToAssembly("int f() { int x = 3 * 4; }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_TRUE(containsLine(asm_out, "mul t0, t0, t1"));
}

TEST(CodeGenTest, Division)
{
    std::string asm_out = compileToAssembly("int f() { int x = 10 / 2; }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_TRUE(containsLine(asm_out, "div t0, t0, t1"));
}

TEST(CodeGenTest, Modulo)
{
    std::string asm_out = compileToAssembly("int f() { int x = 10 % 3; }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_TRUE(containsLine(asm_out, "rem t0, t0, t1"));
}

// ================================================================
//  3. 比较运算
// ================================================================

TEST(CodeGenTest, LessThan)
{
    std::string asm_out = compileToAssembly("int f() { int x = a < b; }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_TRUE(containsLine(asm_out, "slt t0, t0, t1"));
}

TEST(CodeGenTest, LessEqual)
{
    std::string asm_out = compileToAssembly("int f() { int x = a <= b; }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_GE(countInstruction(asm_out, "slt"), 1);
    EXPECT_GE(countInstruction(asm_out, "seqz"), 1);
}

TEST(CodeGenTest, GreaterThan)
{
    std::string asm_out = compileToAssembly("int f() { int x = a > b; }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_TRUE(containsLine(asm_out, "slt t0, t1, t0"));
}

TEST(CodeGenTest, GreaterEqual)
{
    std::string asm_out = compileToAssembly("int f() { int x = a >= b; }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_GE(countInstruction(asm_out, "slt"), 1);
    EXPECT_GE(countInstruction(asm_out, "seqz"), 1);
}

TEST(CodeGenTest, Equal)
{
    std::string asm_out = compileToAssembly("int f() { int x = a == b; }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_GE(countInstruction(asm_out, "sub"), 1);
    EXPECT_GE(countInstruction(asm_out, "seqz"), 1);
}

TEST(CodeGenTest, NotEqual)
{
    std::string asm_out = compileToAssembly("int f() { int x = a != b; }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_GE(countInstruction(asm_out, "sub"), 1);
    EXPECT_GE(countInstruction(asm_out, "snez"), 1);
}

// ================================================================
//  4. 一元运算
// ================================================================

TEST(CodeGenTest, UnaryMinus)
{
    std::string asm_out = compileToAssembly("int f() { int x = -42; }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_TRUE(containsLine(asm_out, "neg t0, t0"));
}

TEST(CodeGenTest, UnaryNot)
{
    std::string asm_out = compileToAssembly("int f() { int x = !a; }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_TRUE(containsLine(asm_out, "seqz t0, t0"));
}

// ================================================================
//  5. 控制流：if 语句
// ================================================================

TEST(CodeGenTest, IfStatement)
{
    std::string asm_out = compileToAssembly("int f() { int x = 0; if (x) { x = 1; } }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_GE(countInstruction(asm_out, "bnez"), 1);
    EXPECT_GE(countInstruction(asm_out, "j "), 1);
    EXPECT_GE(countInstruction(asm_out, ".L"), 1);
}

TEST(CodeGenTest, IfElseStatement)
{
    std::string asm_out = compileToAssembly("int f() { int x = 0; if (x) { x = 1; } else { x = 2; } }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_GE(countInstruction(asm_out, ".L"), 2);
    EXPECT_GE(countInstruction(asm_out, "bnez"), 1);
}

// ================================================================
//  6. 控制流：while 循环
// ================================================================

TEST(CodeGenTest, WhileLoop)
{
    std::string asm_out = compileToAssembly("int f() { int x = 0; while (x < 10) { x = x + 1; } }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_GE(countInstruction(asm_out, ".L"), 2);
    EXPECT_GE(countInstruction(asm_out, "bnez"), 1);
    EXPECT_GE(countInstruction(asm_out, "j "), 1);
    EXPECT_TRUE(containsLine(asm_out, "add t0, t0, t1"));
}

// ================================================================
//  7. 函数调用与返回
// ================================================================

TEST(CodeGenTest, FunctionReturn)
{
    std::string asm_out = compileToAssembly("int f() { return 1; }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_TRUE(containsLine(asm_out, "ret"));
    // return 1 在 IR 中先 ASSIGN 到临时变量，再 RETURN 临时变量
    // 所以生成的是 lw a0, offset(sp) 而非 li a0, 1
    EXPECT_GE(countInstruction(asm_out, "lw"), 1);
}

TEST(CodeGenTest, ProgramExit)
{
    std::string asm_out = compileToAssembly("int f() { return 0; }");
    ASSERT_FALSE(asm_out.empty());
    // crt0.o 负责程序退出，编译器不生成 ecall
    EXPECT_TRUE(containsLine(asm_out, "ret"));
}

// ================================================================
//  8. 汇编结构
// ================================================================

TEST(CodeGenTest, AssemblyHeader)
{
    std::string asm_out = compileToAssembly("int f() { int x = 1; }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_TRUE(containsLine(asm_out, ".text"));
    // crt0.o 提供 _start 入口，编译器只生成函数代码
    EXPECT_TRUE(containsLine(asm_out, "sub sp, sp, t2"));
    EXPECT_GE(countInstruction(asm_out, "sw"), 1);
}

// ================================================================
//  9. 复杂表达式
// ================================================================

TEST(CodeGenTest, ComplexArithmetic)
{
    std::string asm_out = compileToAssembly("int f() { int x = 1 + 2 * 3; }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_TRUE(containsLine(asm_out, "mul t0, t0, t1"));
    EXPECT_TRUE(containsLine(asm_out, "add t0, t0, t1"));
}

TEST(CodeGenTest, NestedExpressions)
{
    std::string asm_out = compileToAssembly("int f() { int x = (1 + 2) * (3 - 4); }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_TRUE(containsLine(asm_out, "add t0, t0, t1"));
    EXPECT_TRUE(containsLine(asm_out, "sub t0, t0, t1"));
    EXPECT_TRUE(containsLine(asm_out, "mul t0, t0, t1"));
}

// ================================================================
//  10. 边界情况
// ================================================================

TEST(CodeGenTest, EmptyFunction)
{
    std::string asm_out = compileToAssembly("void f() { }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_TRUE(containsLine(asm_out, ".text"));
    // void 空函数应生成 ret
    EXPECT_TRUE(containsLine(asm_out, "ret"));
}

TEST(CodeGenTest, MultipleVariables)
{
    std::string asm_out = compileToAssembly("int f() { int a = 1; int b = 2; int c = 3; }");
    ASSERT_FALSE(asm_out.empty());
    EXPECT_GE(countInstruction(asm_out, "li t0,"), 3);
    EXPECT_GE(countInstruction(asm_out, "sw"), 3);
}
