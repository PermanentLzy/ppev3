/**
 * @file test_main.cpp
 * @brief 阶段九：串联测试 —— 端到端编译流程测试
 *
 * 测试从源码到汇编的完整编译管线，模拟 main.cpp 的流程
 *
 * 测试范围：
 *   1. 基础编译（赋值、算术表达式）
 *   2. 控制流（if-else、while）
 *   3. 函数（返回、调用）
 *   4. 优化（常量折叠、死代码删除）
 *   5. 错误处理（语法错误、语义错误、空输入）
 *   6. 复杂程序（多变量、条件分支）
 *   7. 汇编结构（头部、尾部）
 *   8. 端到端集成（基础编译、带优化编译）
 */

#include <gtest/gtest.h>
#include "../src/lexer/Lexer.h"
#include "../src/parser/Parser.h"
#include "../src/parser/ParseError.h"
#include "../src/ast/AST.h"
#include "../src/semantic/SemanticAnalyzer.h"
#include "../src/ir/IRGenerator.h"
#include "../src/optimizer/Optimizer.h"
#include "../src/codegen/CodeGen.h"

#include <sstream>
#include <string>

using namespace MyCompiler;

// ================================================================
//  辅助函数：模拟 main.cpp 的编译流程
// ================================================================

struct CompileResult
{
    bool success = false;
    std::string assembly;
    std::string error;
};

/// 源码 → 汇编（模拟完整编译管线）
static CompileResult compile(const std::string &source, bool enableOpt = false)
{
    CompileResult result;

    // 阶段 1：词法分析
    Lexer lexer(source);

    // 阶段 2：语法分析
    Parser parser(lexer);
    std::unique_ptr<Program> ast;
    try
    {
        ast = parser.parse();
    }
    catch (const ParseError &e)
    {
        result.error = e.what();
        return result;
    }

    // 阶段 4+5：语义分析
    SemanticAnalyzer sema;
    if (sema.analyze(*ast) > 0)
    {
        result.error = "Semantic errors";
        return result;
    }

    // 阶段 6：IR 生成
    IRGenerator irGen;
    auto irProgram = irGen.generate(*ast);

    // 阶段 7：优化
    if (enableOpt)
    {
        Optimizer opt;
        opt.optimize(*irProgram);
    }

    // 阶段 8：代码生成（捕获 stdout）
    std::streambuf *oldCout = std::cout.rdbuf();
    std::ostringstream oss;
    std::cout.rdbuf(oss.rdbuf());

    CodeGen codeGen;
    codeGen.generate(*irProgram);

    std::cout.rdbuf(oldCout);
    result.assembly = oss.str();
    result.success = true;
    return result;
}

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
//  1. 基础编译测试
// ================================================================

TEST(MainTest, BasicAssignment)
{
    auto result = compile("int f() { int x = 42; }");
    ASSERT_TRUE(result.success);
    EXPECT_FALSE(result.assembly.empty());
    EXPECT_TRUE(containsLine(result.assembly, ".text"));
    EXPECT_TRUE(containsLine(result.assembly, ".globl _start"));
    EXPECT_TRUE(containsLine(result.assembly, "li t0, 42"));
}

TEST(MainTest, ArithmeticExpression)
{
    auto result = compile("int f() { int x = 1 + 2 * 3; }");
    ASSERT_TRUE(result.success);
    EXPECT_TRUE(containsLine(result.assembly, "mul t0, t0, t1"));
    EXPECT_TRUE(containsLine(result.assembly, "add t0, t0, t1"));
}

// ================================================================
//  2. 控制流测试
// ================================================================

TEST(MainTest, IfElseStatement)
{
    auto result = compile("int f() { int x = 0; if (x) { x = 1; } else { x = 2; } }");
    ASSERT_TRUE(result.success);
    EXPECT_GE(countInstruction(result.assembly, ".L"), 2);
    EXPECT_GE(countInstruction(result.assembly, "bnez"), 1);
}

TEST(MainTest, WhileLoop)
{
    auto result = compile("int f() { int x = 0; while (x < 10) { x = x + 1; } }");
    ASSERT_TRUE(result.success);
    EXPECT_GE(countInstruction(result.assembly, ".L"), 2);
    EXPECT_GE(countInstruction(result.assembly, "bnez"), 1);
    EXPECT_GE(countInstruction(result.assembly, "j "), 1);
    EXPECT_TRUE(containsLine(result.assembly, "add t0, t0, t1"));
}

// ================================================================
//  3. 函数测试
// ================================================================

TEST(MainTest, FunctionReturn)
{
    auto result = compile("int f() { return 1; }");
    ASSERT_TRUE(result.success);
    EXPECT_TRUE(containsLine(result.assembly, "ret"));
    EXPECT_GE(countInstruction(result.assembly, "lw"), 1);
}

TEST(MainTest, FunctionCall)
{
    auto result = compile("int f() { return 1; } int g() { return f(); }");
    ASSERT_TRUE(result.success);
    EXPECT_GE(countInstruction(result.assembly, "call"), 1);
}

// ================================================================
//  4. 优化测试
// ================================================================

TEST(MainTest, Optimization_ConstFold)
{
    // 1 + 2 应该被优化为 3
    auto result = compile("int f() { int x = 1 + 2; }", true);
    ASSERT_TRUE(result.success);
    EXPECT_TRUE(containsLine(result.assembly, "li t0, 3"));
}

TEST(MainTest, Optimization_DeadCode)
{
    // 死代码应该被删除
    auto result = compile("int f() { int x = 1; int y = 2; return x; }", true);
    ASSERT_TRUE(result.success);
    EXPECT_FALSE(result.assembly.empty());
}

// ================================================================
//  5. 错误处理测试
// ================================================================

TEST(MainTest, SyntaxError)
{
    auto result = compile("int f() { int x = ; }");
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
}

TEST(MainTest, SemanticError_UndeclaredVar)
{
    auto result = compile("int f() { x = 1; }");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, "Semantic errors");
}

TEST(MainTest, EmptyInput)
{
    // 空输入会导致语法错误
    auto result = compile("");
    EXPECT_FALSE(result.success);
}

// ================================================================
//  6. 复杂程序测试
// ================================================================

TEST(MainTest, ComplexProgram)
{
    std::string code = R"(
int f() {
    int x = 10;
    int y = 20;
    int z = x + y;
    if (z > 25) {
        z = z - 5;
    }
    return z;
}
)";
    auto result = compile(code);
    ASSERT_TRUE(result.success);
    EXPECT_TRUE(containsLine(result.assembly, ".text"));
    EXPECT_TRUE(containsLine(result.assembly, "ret"));
    EXPECT_GE(countInstruction(result.assembly, "add"), 1);
    EXPECT_GE(countInstruction(result.assembly, "sub"), 1);
}

// ================================================================
//  7. 汇编结构测试
// ================================================================

TEST(MainTest, AssemblyStructure)
{
    auto result = compile("int f() { int x = 1; }");
    ASSERT_TRUE(result.success);
    EXPECT_TRUE(containsLine(result.assembly, ".text"));
    EXPECT_TRUE(containsLine(result.assembly, ".globl _start"));
    EXPECT_TRUE(containsLine(result.assembly, "_start:"));
    EXPECT_TRUE(containsLine(result.assembly, "addi sp, sp, -256"));
    EXPECT_TRUE(containsLine(result.assembly, "ecall"));
    EXPECT_TRUE(containsLine(result.assembly, "li a7, 93"));
}

// ================================================================
//  8. 端到端集成测试
// ================================================================

TEST(MainTest, EndToEnd_Simple)
{
    // 模拟：echo "int f() { return 0; }" | mycompiler
    auto result = compile("int f() { return 0; }");
    ASSERT_TRUE(result.success);
    EXPECT_TRUE(containsLine(result.assembly, "ecall"));
    EXPECT_TRUE(containsLine(result.assembly, "li a7, 93"));
}

TEST(MainTest, EndToEnd_WithOpt)
{
    // 模拟：echo "int f() { int x = 1 + 2; return x; }" | mycompiler -opt
    auto result = compile("int f() { int x = 1 + 2; return x; }", true);
    ASSERT_TRUE(result.success);
    // 优化后 1+2 应该变成 3
    EXPECT_TRUE(containsLine(result.assembly, "li t0, 3"));
}
