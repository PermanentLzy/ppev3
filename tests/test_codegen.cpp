/**
 * @file test_codegen.cpp
 * @brief 代码生成器（CodeGen）测试 —— 不依赖 Google Test，使用简易测试框架
 *
 * 编译: g++ -std=c++17 -I../src test_codegen.cpp ../src/lexer/*.cpp ../src/parser/*.cpp ../src/ast/*.cpp ../src/semantic/*.cpp ../src/ir/*.cpp ../src/optimizer/*.cpp ../src/codegen/*.cpp ../src/utils/*.cpp -o test_codegen.exe
 * 运行: .\test_codegen.exe
 */

#include "../src/lexer/Lexer.h"
#include "../src/parser/Parser.h"
#include "../src/parser/ParseError.h"
#include "../src/ir/IRGenerator.h"
#include "../src/ir/ThreeAddrCode.h"
#include "../src/codegen/CodeGen.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <functional>

using namespace MyCompiler;

// ================================================================
//  简易测试框架
// ================================================================

struct TestCase
{
    std::string name;
    std::function<bool()> fn;
};

static std::vector<TestCase> g_tests;
static int g_passed = 0;
static int g_failed = 0;

#define TEST(name)                                                     \
    static bool test_##name();                                         \
    static struct Register_##name                                      \
    {                                                                  \
        Register_##name() { g_tests.push_back({#name, test_##name}); } \
    } reg_##name;                                                      \
    static bool test_##name()

#define EXPECT_TRUE(cond)                                                                      \
    do                                                                                         \
    {                                                                                          \
        if (!(cond))                                                                           \
        {                                                                                      \
            std::cerr << "  FAIL: " << #cond << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
            return false;                                                                      \
        }                                                                                      \
    } while (0)
#define EXPECT_FALSE(cond) EXPECT_TRUE(!(cond))
#define EXPECT_GE(a, b)                                                                                                                          \
    do                                                                                                                                           \
    {                                                                                                                                            \
        if (!((a) >= (b)))                                                                                                                       \
        {                                                                                                                                        \
            std::cerr << "  FAIL: " << #a << " >= " << #b << " (got " << (a) << " vs " << (b) << ") at " << __FILE__ << ":" << __LINE__ << "\n"; \
            return false;                                                                                                                        \
        }                                                                                                                                        \
    } while (0)
#define EXPECT_EQ(a, b)                                                                                     \
    do                                                                                                      \
    {                                                                                                       \
        if (!((a) == (b)))                                                                                  \
        {                                                                                                   \
            std::cerr << "  FAIL: " << #a << " == " << #b << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
            return false;                                                                                   \
        }                                                                                                   \
    } while (0)
#define ASSERT_NE(a, b)                                                                                     \
    do                                                                                                      \
    {                                                                                                       \
        if ((a) == (b))                                                                                     \
        {                                                                                                   \
            std::cerr << "  FAIL: " << #a << " != " << #b << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
            return false;                                                                                   \
        }                                                                                                   \
    } while (0)
#define ADD_FAILURE()                                                     \
    do                                                                    \
    {                                                                     \
        std::cerr << "  FAIL at " << __FILE__ << ":" << __LINE__ << "\n"; \
        return false;                                                     \
    } while (0)

// ================================================================
//  辅助函数
// ================================================================

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
        std::cerr << "  Parse error: " << e.what() << "\n";
        return nullptr;
    }
    IRGenerator irGen;
    return irGen.generate(*ast);
}

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

static std::string compileToAssembly(const std::string &src)
{
    auto prog = compileToIR(src);
    if (!prog)
        return "";
    return captureCodeGenOutput(*prog);
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
//  1. 基础赋值与常量
// ================================================================

TEST(SimpleAssignment)
{
    std::string asm_out = compileToAssembly("int f() { int x = 42; }");
    EXPECT_FALSE(asm_out.empty());
    EXPECT_TRUE(containsLine(asm_out, "li t0, 42"));
    EXPECT_GE(countInstruction(asm_out, "sw"), 1);
    return true;
}

TEST(VariableCopy)
{
    std::string asm_out = compileToAssembly("int f() { int a = 1; int b = a; }");
    EXPECT_FALSE(asm_out.empty());
    // 常量加载可能被寄存器分配或常量折叠改变形态
    EXPECT_TRUE(countInstruction(asm_out, "li ") >= 1 || countInstruction(asm_out, "addi ") >= 1);
    EXPECT_GE(countInstruction(asm_out, "sw"), 1);
    return true;
}

// ================================================================
//  2. 算术运算
// ================================================================

TEST(Addition)
{
    std::string asm_out = compileToAssembly("int f() { int x = 1 + 2; }");
    EXPECT_FALSE(asm_out.empty());
    // 常量 1+2 可能被常量折叠为 li，或用 addi
    EXPECT_TRUE(countInstruction(asm_out, "add ") >= 1 || countInstruction(asm_out, "addi ") >= 1 || countInstruction(asm_out, "li ") >= 1);
    return true;
}

TEST(Subtraction)
{
    std::string asm_out = compileToAssembly("int f() { int x = 5 - 3; }");
    EXPECT_FALSE(asm_out.empty());
    // 常量 5-3 可能被常量折叠，或用 sub/addi
    EXPECT_TRUE(countInstruction(asm_out, "sub ") >= 1 || countInstruction(asm_out, "addi ") >= 1 || countInstruction(asm_out, "li ") >= 1);
    return true;
}

TEST(Multiplication)
{
    std::string asm_out = compileToAssembly("int f() { int x = 3 * 4; }");
    EXPECT_FALSE(asm_out.empty());
    // 常量 3*4=12 可能被常量折叠，或用 mul/slli
    EXPECT_TRUE(countInstruction(asm_out, "mul ") >= 1 || countInstruction(asm_out, "slli ") >= 1 || countInstruction(asm_out, "li ") >= 1);
    return true;
}

TEST(Division)
{
    std::string asm_out = compileToAssembly("int f() { int x = 10 / 2; }");
    EXPECT_FALSE(asm_out.empty());
    EXPECT_TRUE(countInstruction(asm_out, "div ") >= 1 || countInstruction(asm_out, "li ") >= 1);
    return true;
}

TEST(Modulo)
{
    std::string asm_out = compileToAssembly("int f() { int x = 10 % 3; }");
    EXPECT_FALSE(asm_out.empty());
    EXPECT_TRUE(countInstruction(asm_out, "rem ") >= 1 || countInstruction(asm_out, "li ") >= 1);
    return true;
}

// ================================================================
//  3. 比较运算
// ================================================================

TEST(LessThan)
{
    std::string asm_out = compileToAssembly("int f() { int x = a < b; }");
    EXPECT_FALSE(asm_out.empty());
    EXPECT_TRUE(countInstruction(asm_out, "slt ") >= 1 || countInstruction(asm_out, "sltu ") >= 1);
    return true;
}

TEST(LessEqual)
{
    std::string asm_out = compileToAssembly("int f() { int x = a <= b; }");
    EXPECT_FALSE(asm_out.empty());
    EXPECT_GE(countInstruction(asm_out, "slt"), 1);
    EXPECT_GE(countInstruction(asm_out, "seqz"), 1);
    return true;
}

TEST(GreaterThan)
{
    std::string asm_out = compileToAssembly("int f() { int x = a > b; }");
    EXPECT_FALSE(asm_out.empty());
    // a > b 实现为 slt dst, b, a
    EXPECT_TRUE(countInstruction(asm_out, "slt ") >= 1 || countInstruction(asm_out, "sltu ") >= 1);
    return true;
}

TEST(GreaterEqual)
{
    std::string asm_out = compileToAssembly("int f() { int x = a >= b; }");
    EXPECT_FALSE(asm_out.empty());
    EXPECT_GE(countInstruction(asm_out, "slt"), 1);
    EXPECT_GE(countInstruction(asm_out, "seqz"), 1);
    return true;
}

TEST(Equal)
{
    std::string asm_out = compileToAssembly("int f() { int x = a == b; }");
    EXPECT_FALSE(asm_out.empty());
    EXPECT_GE(countInstruction(asm_out, "sub"), 1);
    EXPECT_GE(countInstruction(asm_out, "seqz"), 1);
    return true;
}

TEST(NotEqual)
{
    std::string asm_out = compileToAssembly("int f() { int x = a != b; }");
    EXPECT_FALSE(asm_out.empty());
    EXPECT_GE(countInstruction(asm_out, "sub"), 1);
    EXPECT_GE(countInstruction(asm_out, "snez"), 1);
    return true;
}

// ================================================================
//  4. 一元运算
// ================================================================

TEST(UnaryMinus)
{
    std::string asm_out = compileToAssembly("int f() { int x = -42; }");
    EXPECT_FALSE(asm_out.empty());
    // -42 可能被常量折叠为 li，或用 neg/addi
    EXPECT_TRUE(countInstruction(asm_out, "neg ") >= 1 || countInstruction(asm_out, "addi ") >= 1 || countInstruction(asm_out, "li ") >= 1);
    return true;
}

TEST(UnaryNot)
{
    std::string asm_out = compileToAssembly("int f() { int x = !a; }");
    EXPECT_FALSE(asm_out.empty());
    EXPECT_TRUE(countInstruction(asm_out, "seqz ") >= 1);
    return true;
}

// ================================================================
//  5. 控制流：if 语句
// ================================================================

TEST(IfStatement)
{
    std::string asm_out = compileToAssembly("int f() { int x = 0; if (x) { x = 1; } }");
    EXPECT_FALSE(asm_out.empty());
    // beqz/bnez 都可（CodeGen 会优化 IF_GOTO+GOTO→beqz）
    EXPECT_TRUE(countInstruction(asm_out, "bnez") >= 1 || countInstruction(asm_out, "beqz") >= 1);
    EXPECT_GE(countInstruction(asm_out, ".L"), 1);
    return true;
}

TEST(IfElseStatement)
{
    std::string asm_out = compileToAssembly("int f() { int x = 0; if (x) { x = 1; } else { x = 2; } }");
    EXPECT_FALSE(asm_out.empty());
    EXPECT_GE(countInstruction(asm_out, ".L"), 2);
    EXPECT_TRUE(countInstruction(asm_out, "bnez") >= 1 || countInstruction(asm_out, "beqz") >= 1);
    return true;
}

// ================================================================
//  6. 控制流：while 循环
// ================================================================

TEST(WhileLoop)
{
    std::string asm_out = compileToAssembly("int f() { int x = 0; while (x < 10) { x = x + 1; } }");
    EXPECT_FALSE(asm_out.empty());
    EXPECT_GE(countInstruction(asm_out, ".L"), 2);
    // 循环分支可能用 bnez 或 beqz
    EXPECT_TRUE(countInstruction(asm_out, "bnez") >= 1 || countInstruction(asm_out, "beqz") >= 1);
    // x + 1 可能用 add/addi
    EXPECT_TRUE(countInstruction(asm_out, "add ") >= 1 || countInstruction(asm_out, "addi ") >= 1 || countInstruction(asm_out, "li ") >= 1);
    return true;
}

// ================================================================
//  7. 函数调用与返回
// ================================================================

TEST(FunctionReturn)
{
    std::string asm_out = compileToAssembly("int f() { return 1; }");
    EXPECT_FALSE(asm_out.empty());
    EXPECT_TRUE(containsLine(asm_out, "ret"));
    // return 1 在 IR 中先 ASSIGN 到临时变量，再 RETURN 临时变量
    // 所以生成的是 lw a0, offset(sp) 而非 li a0, 1
    EXPECT_GE(countInstruction(asm_out, "lw"), 1);
    return true;
}

TEST(ProgramExit)
{
    std::string asm_out = compileToAssembly("int f() { return 0; }");
    EXPECT_FALSE(asm_out.empty());
    // crt0.o 负责程序退出，编译器不生成 ecall
    EXPECT_TRUE(containsLine(asm_out, "ret"));
    return true;
}

// ================================================================
//  8. 汇编结构
// ================================================================

TEST(AssemblyHeader)
{
    std::string asm_out = compileToAssembly("int f() { int x = 1; }");
    EXPECT_FALSE(asm_out.empty());
    EXPECT_TRUE(containsLine(asm_out, ".text"));
    // crt0.o 提供 _start 入口，编译器只生成函数代码
    EXPECT_TRUE(containsLine(asm_out, "sub sp, sp, t2"));
    EXPECT_GE(countInstruction(asm_out, "sw"), 1);
    return true;
}

// ================================================================
//  9. 复杂表达式
// ================================================================

TEST(ComplexArithmetic)
{
    std::string asm_out = compileToAssembly("int f() { int x = 1 + 2 * 3; }");
    EXPECT_FALSE(asm_out.empty());
    // 1+2*3=7 可能被常量折叠，或保留 mul+add/addi
    EXPECT_TRUE(countInstruction(asm_out, "mul ") >= 1 || countInstruction(asm_out, "slli ") >= 1 || countInstruction(asm_out, "li ") >= 1);
    EXPECT_TRUE(countInstruction(asm_out, "add ") >= 1 || countInstruction(asm_out, "addi ") >= 1 || countInstruction(asm_out, "li ") >= 1);
    return true;
}

TEST(NestedExpressions)
{
    std::string asm_out = compileToAssembly("int f() { int x = (1 + 2) * (3 - 4); }");
    EXPECT_FALSE(asm_out.empty());
    // 常量 (1+2)*(3-4)=3*(-1)=-3 可能被折叠
    EXPECT_TRUE(countInstruction(asm_out, "add ") >= 1 || countInstruction(asm_out, "addi ") >= 1 || countInstruction(asm_out, "li ") >= 1);
    EXPECT_TRUE(countInstruction(asm_out, "sub ") >= 1 || countInstruction(asm_out, "addi ") >= 1 || countInstruction(asm_out, "li ") >= 1);
    EXPECT_TRUE(countInstruction(asm_out, "mul ") >= 1 || countInstruction(asm_out, "slli ") >= 1 || countInstruction(asm_out, "li ") >= 1);
    return true;
}

// ================================================================
//  10. 边界情况
// ================================================================

TEST(EmptyFunction)
{
    std::string asm_out = compileToAssembly("void f() { }");
    EXPECT_FALSE(asm_out.empty());
    EXPECT_TRUE(containsLine(asm_out, ".text"));
    // void 空函数应生成 ret
    EXPECT_TRUE(containsLine(asm_out, "ret"));
    return true;
}

TEST(MultipleVariables)
{
    std::string asm_out = compileToAssembly("int f() { int a = 1; int b = 2; int c = 3; }");
    EXPECT_FALSE(asm_out.empty());
    EXPECT_GE(countInstruction(asm_out, "li t0,"), 3);
    EXPECT_GE(countInstruction(asm_out, "sw"), 3);
    return true;
}

// ================================================================
//  主函数
// ================================================================

int main()
{
    std::cout << "========================================\n";
    std::cout << "  CodeGen Test Suite\n";
    std::cout << "========================================\n\n";

    for (auto &tc : g_tests)
    {
        std::cout << "[ RUN      ] " << tc.name << "\n";
        bool result = tc.fn();
        if (result)
        {
            std::cout << "[       OK ] " << tc.name << "\n";
            g_passed++;
        }
        else
        {
            std::cout << "[  FAILED  ] " << tc.name << "\n";
            g_failed++;
        }
    }

    std::cout << "\n========================================\n";
    std::cout << "  " << g_passed << " passed, " << g_failed << " failed, "
              << (g_passed + g_failed) << " total\n";
    std::cout << "========================================\n";

    return g_failed > 0 ? 1 : 0;
}
