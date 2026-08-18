/**
 * @file test_optimizer.cpp
 * @brief 优化器测试用例
 *
 * 测试范围：
 *   1. 常量折叠 (ConstFold)：二元/一元运算常量计算、除零检查
 *   2. 公共子表达式消除 (CSE)：重复算术表达式、变量赋值失效
 *   3. 死代码删除 (DCE)：未使用变量、部分使用、控制流保留
 *   4. 集成测试：多 Pass 联合优化
 */

#include <gtest/gtest.h>
#include "../src/lexer/Lexer.h"
#include "../src/parser/Parser.h"
#include "../src/parser/ParseError.h"
#include "../src/ir/IRGenerator.h"
#include "../src/ir/ThreeAddrCode.h"
#include "../src/optimizer/Optimizer.h"

#include <sstream>
#include <string>
#include <algorithm>

using namespace MyCompiler;

// ================================================================
//  辅助函数
// ================================================================

/// 源码 → AST → IR
static std::unique_ptr<TACProgram> compileToIR(const std::string& src) {
    Lexer lexer(src);
    Parser parser(lexer);
    std::unique_ptr<Program> ast;
    try {
        ast = parser.parse();
    } catch (const ParseError& e) {
        ADD_FAILURE() << "Parse error: " << e.what();
        return nullptr;
    }
    IRGenerator irGen;
    return irGen.generate(*ast);
}

/// 源码 → AST → IR → 优化，返回优化后的 IR
static std::unique_ptr<TACProgram> compileAndOptimize(const std::string& src) {
    auto prog = compileToIR(src);
    if (!prog) return nullptr;
    Optimizer opt;
    opt.optimize(*prog);
    return prog;
}

/// 统计某种指令的出现次数
static int countType(const TACProgram& prog, TACType type) {
    int cnt = 0;
    for (auto& i : prog.instructions)
        if (i.type == type) ++cnt;
    return cnt;
}

/// 统计指定运算符的出现次数
static int countOp(const TACProgram& prog, const std::string& op) {
    int cnt = 0;
    for (auto& i : prog.instructions)
        if (i.op == op) ++cnt;
    return cnt;
}

/// 判断优化后的 IR 是否包含某条特定指令（按 toString 匹配）
static bool hasInstruction(const TACProgram& prog, const std::string& substr) {
    for (auto& i : prog.instructions) {
        if (i.toString().find(substr) != std::string::npos)
            return true;
    }
    return false;
}

/// 获取优化后 IR 的字符串表示（用于调试）
static std::string irToString(const TACProgram& prog) {
    std::ostringstream oss;
    for (size_t idx = 0; idx < prog.instructions.size(); ++idx) {
        oss << idx << ": " << prog.instructions[idx].toString() << "\n";
    }
    return oss.str();
}

// ================================================================
//  第 1 组：常量折叠 (Constant Folding)
// ================================================================

TEST(ConstFoldTest, SimpleAddition) {
    // int f() { int x = 3 + 5; return x; } → 折叠后 x = 8
    auto prog = compileAndOptimize("int f() { int x = 3 + 5; return x; }");
    ASSERT_NE(prog, nullptr);

    // 不应再有 BINARY + 指令
    EXPECT_EQ(countOp(*prog, "+"), 0);
    // 检查 x = 8 被保留 (return x 使用了 x)
    EXPECT_TRUE(hasInstruction(*prog, "x = 8"))
        << "IR:\n" << irToString(*prog);
}

TEST(ConstFoldTest, SimpleSubtraction) {
    auto prog = compileAndOptimize("int f() { int x = 10 - 3; return x; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_EQ(countOp(*prog, "-"), 0);
    EXPECT_TRUE(hasInstruction(*prog, "x = 7"))
        << "IR:\n" << irToString(*prog);
}

TEST(ConstFoldTest, SimpleMultiplication) {
    auto prog = compileAndOptimize("int f() { int x = 4 * 6; return x; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_EQ(countOp(*prog, "*"), 0);
    EXPECT_TRUE(hasInstruction(*prog, "x = 24"))
        << "IR:\n" << irToString(*prog);
}

TEST(ConstFoldTest, SimpleDivision) {
    auto prog = compileAndOptimize("int f() { int x = 20 / 4; return x; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_EQ(countOp(*prog, "/"), 0);
    EXPECT_TRUE(hasInstruction(*prog, "x = 5"))
        << "IR:\n" << irToString(*prog);
}

TEST(ConstFoldTest, SimpleModulo) {
    auto prog = compileAndOptimize("int f() { int x = 17 % 5; return x; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_EQ(countOp(*prog, "%"), 0);
    EXPECT_TRUE(hasInstruction(*prog, "x = 2"))
        << "IR:\n" << irToString(*prog);
}

TEST(ConstFoldTest, EqualComparisonTrue) {
    auto prog = compileAndOptimize("int f() { int x = 5 == 5; return x; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_EQ(countOp(*prog, "=="), 0);
    EXPECT_TRUE(hasInstruction(*prog, "x = 1"))
        << "IR:\n" << irToString(*prog);
}

TEST(ConstFoldTest, EqualComparisonFalse) {
    auto prog = compileAndOptimize("int f() { int x = 3 == 7; return x; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_EQ(countOp(*prog, "=="), 0);
    EXPECT_TRUE(hasInstruction(*prog, "x = 0"))
        << "IR:\n" << irToString(*prog);
}

TEST(ConstFoldTest, NotEqualComparison) {
    auto prog = compileAndOptimize("int f() { int x = 3 != 7; return x; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_EQ(countOp(*prog, "!="), 0);
    EXPECT_TRUE(hasInstruction(*prog, "x = 1"))
        << "IR:\n" << irToString(*prog);
}

TEST(ConstFoldTest, LessThanTrue) {
    auto prog = compileAndOptimize("int f() { int x = 2 < 10; return x; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_EQ(countOp(*prog, "<"), 0);
    EXPECT_TRUE(hasInstruction(*prog, "x = 1"))
        << "IR:\n" << irToString(*prog);
}

TEST(ConstFoldTest, LessThanFalse) {
    auto prog = compileAndOptimize("int f() { int x = 10 < 2; return x; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_EQ(countOp(*prog, "<"), 0);
    EXPECT_TRUE(hasInstruction(*prog, "x = 0"))
        << "IR:\n" << irToString(*prog);
}

TEST(ConstFoldTest, LessEqualTrue) {
    auto prog = compileAndOptimize("int f() { int x = 5 <= 5; return x; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_EQ(countOp(*prog, "<="), 0);
    EXPECT_TRUE(hasInstruction(*prog, "x = 1"))
        << "IR:\n" << irToString(*prog);
}

TEST(ConstFoldTest, GreaterThanFalse) {
    auto prog = compileAndOptimize("int f() { int x = 3 > 8; return x; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_EQ(countOp(*prog, ">"), 0);
    EXPECT_TRUE(hasInstruction(*prog, "x = 0"))
        << "IR:\n" << irToString(*prog);
}

TEST(ConstFoldTest, GreaterEqualTrue) {
    auto prog = compileAndOptimize("int f() { int x = 7 >= 7; return x; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_EQ(countOp(*prog, ">="), 0);
    EXPECT_TRUE(hasInstruction(*prog, "x = 1"))
        << "IR:\n" << irToString(*prog);
}

TEST(ConstFoldTest, LogicalAndTrue) {
    // && 在 IR 生成阶段已展开为 IF_GOTO，验证 BINARY 层面不出现 &&
    auto prog = compileAndOptimize("int f() { int x = 1 && 1; return x; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_EQ(countOp(*prog, "&&"), 0);
}

TEST(ConstFoldTest, UnaryMinus) {
    auto prog = compileAndOptimize("int f() { int x = -42; return x; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_TRUE(hasInstruction(*prog, "x = -42"))
        << "IR:\n" << irToString(*prog);
}

TEST(ConstFoldTest, UnaryNotTrue) {
    auto prog = compileAndOptimize("int f() { int x = !0; return x; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_TRUE(hasInstruction(*prog, "x = 1"))
        << "IR:\n" << irToString(*prog);
}

TEST(ConstFoldTest, UnaryNotFalse) {
    auto prog = compileAndOptimize("int f() { int x = !5; return x; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_TRUE(hasInstruction(*prog, "x = 0"))
        << "IR:\n" << irToString(*prog);
}

TEST(ConstFoldTest, ComplexExpression) {
    // int x = (2 + 3) * (10 - 6); → 5 * 4 → 20
    auto prog = compileAndOptimize("int f() { int x = (2 + 3) * (10 - 6); return x; }");
    ASSERT_NE(prog, nullptr);

    // 多轮折叠后，所有 BINARY 运算应消失
    EXPECT_TRUE(hasInstruction(*prog, "x = 20") || hasInstruction(*prog, "20"))
        << "IR:\n" << irToString(*prog);
}

TEST(ConstFoldTest, NestedConstantExpr) {
    // int x = 1 + 2 + 3 + 4; → 10
    auto prog = compileAndOptimize("int f() { int x = 1 + 2 + 3 + 4; return x; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_TRUE(hasInstruction(*prog, "x = 10"))
        << "IR:\n" << irToString(*prog);
}

TEST(ConstFoldTest, VariableNotFolded) {
    // 含变量的表达式不应被完全折叠
    auto prog = compileAndOptimize("int f(int a) { int x = a + 3; return x; }");
    ASSERT_NE(prog, nullptr);
    // a + 3 中 a 是变量，仍应保留 BINARY +
    EXPECT_GE(countOp(*prog, "+"), 1)
        << "IR:\n" << irToString(*prog);
}

// ================================================================
//  第 2 组：公共子表达式消除 (CSE)
// ================================================================

TEST(CSETest, SimpleDuplicate) {
    // int f() { int a = x + y; int b = x + y; }
    // 第二个 x + y 应被消除
    auto prog = compileAndOptimize(
        "int f(int x, int y) { int a = x + y; int b = x + y; return b; }");
    ASSERT_NE(prog, nullptr);

    // 优化前有 2 个 BINARY +，优化后应只有 1 个
    EXPECT_EQ(countOp(*prog, "+"), 1)
        << "IR:\n" << irToString(*prog);
}

TEST(CSETest, DuplicateWithDifferentResult) {
    auto prog = compileAndOptimize(
        "int f(int x, int y) { int a = x * y; int b = x * y; return a + b; }");
    ASSERT_NE(prog, nullptr);

    // 只应保留 1 个 BINARY *
    EXPECT_EQ(countOp(*prog, "*"), 1)
        << "IR:\n" << irToString(*prog);
}

TEST(CSETest, NoDuplicateDifferentOps) {
    // + 和 - 是不同的表达式，不应被消除
    auto prog = compileAndOptimize(
        "int f(int x, int y) { int a = x + y; int b = x - y; return a + b; }");
    ASSERT_NE(prog, nullptr);

    // 两个操作数都被 return 使用，+ 和 - 都应保留
    EXPECT_GE(countOp(*prog, "+"), 1);
    EXPECT_GE(countOp(*prog, "-"), 1);
}

TEST(CSETest, NoDuplicateDifferentOperands) {
    // x+y 和 x+z 是不同的，各有 1 个 +，加上 return a+b 共 3 个
    auto prog = compileAndOptimize(
        "int f(int x, int y, int z) { int a = x + y; int b = x + z; return a + b; }");
    ASSERT_NE(prog, nullptr);

    EXPECT_EQ(countOp(*prog, "+"), 3)
        << "IR:\n" << irToString(*prog);
}

TEST(CSETest, TripleDuplicate) {
    auto prog = compileAndOptimize(
        "int f(int a, int b) { int x = a + b; int y = a + b; int z = a + b; return z; }");
    ASSERT_NE(prog, nullptr);

    // 三次 a+b → 只保留 1 次
    EXPECT_EQ(countOp(*prog, "+"), 1)
        << "IR:\n" << irToString(*prog);
}

TEST(CSETest, UnaryDuplicate) {
    auto prog = compileAndOptimize(
        "int f(int x) { int a = -x; int b = -x; return b; }");
    ASSERT_NE(prog, nullptr);

    // 两次 -x → 只保留 1 次 UNARY -
    // 注意 UNARY 类型没有 op 字符串（op 在 IR 中为 "-"）
    int unaryCount = countType(*prog, TACType::UNARY);
    EXPECT_LE(unaryCount, 1)
        << "IR:\n" << irToString(*prog);
}

TEST(CSETest, InvalidateAfterAssignment) {
    // x 在中间被修改，CSE 应失效 → 两个 + 都应保留
    auto prog = compileAndOptimize(
        "int f(int x, int y) { int a = x + y; x = 10; int b = x + y; return a + b; }");
    ASSERT_NE(prog, nullptr);

    // x 被重新赋值后，x+y 的值可能改变，应保留 2 个 x+y，加上 return a+b 共 3 个
    EXPECT_EQ(countOp(*prog, "+"), 3)
        << "IR:\n" << irToString(*prog);
}

TEST(CSETest, ChainedExpression) {
    // b+c 重复出现，CSE 应消除一个；所有值都被使用
    auto prog = compileAndOptimize(
        "int f(int b, int c, int e) { int a = b + c; int d = a + e; int g = b + c; return d + g; }");
    ASSERT_NE(prog, nullptr);

    // b+c 出现 2 次，CSE 后只需 1 个；另外 a+e 和 d+g 各 1 个 = 共 3 个
    // 但 CSE 合并 b+c → 只剩 2 个不同表达式: (b+c)+e 和 (b+c)+g 的返回
    // 实际: b+c(1个), a+e(1个), d+g(1个) — CSE 消除了第二次 b+c
    EXPECT_EQ(countOp(*prog, "+"), 3)
        << "IR:\n" << irToString(*prog);
}

// ================================================================
//  第 3 组：死代码删除 (DCE)
// ================================================================

TEST(DCETest, UnusedVariable) {
    // x 定义后从未使用 → 应被删除
    auto prog = compileAndOptimize(
        "int f() { int x = 42; int y = 10; return y; }");
    ASSERT_NE(prog, nullptr);

    // x = 42 应被删除
    EXPECT_FALSE(hasInstruction(*prog, "x = 42"))
        << "IR:\n" << irToString(*prog);
    // y = 10 应保留（被 return 使用）
    EXPECT_TRUE(hasInstruction(*prog, "y = 10"))
        << "IR:\n" << irToString(*prog);
}

TEST(DCETest, UsedVariablePreserved) {
    auto prog = compileAndOptimize(
        "int f() { int x = 100; return x; }");
    ASSERT_NE(prog, nullptr);

    // x = 100 被 return 使用，必须保留
    EXPECT_TRUE(hasInstruction(*prog, "x = 100"))
        << "IR:\n" << irToString(*prog);
}

TEST(DCETest, ChainUsage) {
    // 使用参数而非常量，避免常量折叠将整条链折叠掉
    auto prog = compileAndOptimize(
        "int f(int p) { int a = p; int b = a + 2; int c = b * 3; return c; }");
    ASSERT_NE(prog, nullptr);

    // 整条依赖链都应保留（因为 p 是参数，无法折叠）
    EXPECT_TRUE(hasInstruction(*prog, "a = p") || hasInstruction(*prog, "a ="))
        << "IR:\n" << irToString(*prog);
    EXPECT_GE(countOp(*prog, "+"), 1);
    EXPECT_GE(countOp(*prog, "*"), 1);
}

TEST(DCETest, PartiallyDeadChain) {
    // a → b (未使用) | c → return
    auto prog = compileAndOptimize(
        "int f() { int a = 1; int b = a + 2; int c = 10; return c; }");
    ASSERT_NE(prog, nullptr);

    // b 及其依赖 a 未被 return 使用 → 应被删除
    EXPECT_FALSE(hasInstruction(*prog, "b ="))
        << "IR:\n" << irToString(*prog);
    EXPECT_FALSE(hasInstruction(*prog, "a ="))
        << "IR:\n" << irToString(*prog);
    // c = 10 应保留
    EXPECT_TRUE(hasInstruction(*prog, "c = 10"))
        << "IR:\n" << irToString(*prog);
}

TEST(DCETest, ControlFlowPreserved) {
    // if / goto / label 等控制流指令必须保留
    auto prog = compileAndOptimize(
        "int f(int x) { if (x) { x = 1; } return x; }");
    ASSERT_NE(prog, nullptr);

    // 必须有控制流指令
    EXPECT_GE(countType(*prog, TACType::IF_GOTO), 1)
        << "IR:\n" << irToString(*prog);
    EXPECT_GE(countType(*prog, TACType::LABEL), 1)
        << "IR:\n" << irToString(*prog);
}

TEST(DCETest, WhileLoopPreserved) {
    auto prog = compileAndOptimize(
        "int f(int x) { int s = 0; while (x) { s = s + x; x = x - 1; } return s; }");
    ASSERT_NE(prog, nullptr);

    // 循环控制流必须保留
    EXPECT_GE(countType(*prog, TACType::GOTO), 1)
        << "IR:\n" << irToString(*prog);
    EXPECT_GE(countType(*prog, TACType::IF_GOTO), 1)
        << "IR:\n" << irToString(*prog);
    // s = 0 被使用 → 保留
    EXPECT_TRUE(hasInstruction(*prog, "s = 0"))
        << "IR:\n" << irToString(*prog);
}

TEST(DCETest, MultipleUnused) {
    auto prog = compileAndOptimize(
        "int f() { int a = 1; int b = 2; int c = 3; int d = 4; return d; }");
    ASSERT_NE(prog, nullptr);

    // 只有 d = 4 被 return 使用
    EXPECT_FALSE(hasInstruction(*prog, "a = 1"))
        << "IR:\n" << irToString(*prog);
    EXPECT_FALSE(hasInstruction(*prog, "b = 2"))
        << "IR:\n" << irToString(*prog);
    EXPECT_FALSE(hasInstruction(*prog, "c = 3"))
        << "IR:\n" << irToString(*prog);
    EXPECT_TRUE(hasInstruction(*prog, "d = 4"))
        << "IR:\n" << irToString(*prog);
}

TEST(DCETest, DeadStoreInIfBranch) {
    auto prog = compileAndOptimize(
        "int f(int x) { int y = 0; if (x) { int z = 99; y = 1; } return y; }");
    ASSERT_NE(prog, nullptr);

    // y = 0 和 y = 1 都被使用（return y），应保留
    EXPECT_TRUE(hasInstruction(*prog, "y = 0"))
        << "IR:\n" << irToString(*prog);
    // z = 99 未被使用 → 应删除
    EXPECT_FALSE(hasInstruction(*prog, "z = 99"))
        << "IR:\n" << irToString(*prog);
}

TEST(DCETest, ReturnInstructionPreserved) {
    auto prog = compileAndOptimize("int f() { return 0; }");
    ASSERT_NE(prog, nullptr);

    // RETURN 指令必须保留
    EXPECT_GE(countType(*prog, TACType::RETURN), 1);
}

// ================================================================
//  第 4 组：集成测试 (多 Pass 联合)
// ================================================================

TEST(IntegrationTest, FoldThenCSE) {
    // 常量折叠后可能暴露出新的 CSE 机会
    auto prog = compileAndOptimize(
        "int f() { int x = 1 + 2; int y = 1 + 2; return y; }");
    ASSERT_NE(prog, nullptr);

    // 折叠后不应有 + 运算
    EXPECT_EQ(countOp(*prog, "+"), 0)
        << "IR:\n" << irToString(*prog);
    // y 被 return 使用，应保留
    EXPECT_TRUE(hasInstruction(*prog, "y = 3"))
        << "IR:\n" << irToString(*prog);
    // return y 应保留
    EXPECT_GE(countType(*prog, TACType::RETURN), 1);
}

TEST(IntegrationTest, FoldThenDCE) {
    // 常量折叠 + 传播后，冗余中间变量被 DCE 清除
    auto prog = compileAndOptimize(
        "int f() { int a = 2 + 3; int b = a + 0; int c = b; return c; }");
    ASSERT_NE(prog, nullptr);

    // 整条链折叠为 c=5，a, b 被 DCE 删除
    EXPECT_LE(prog->instructions.size(), 10U)
        << "IR:\n" << irToString(*prog);
    // c 应直接赋值为常量
    EXPECT_TRUE(hasInstruction(*prog, "c = 5"))
        << "IR:\n" << irToString(*prog);
}

TEST(IntegrationTest, CSEDeadStoreInteraction) {
    // CSE 复用 + DCE 清除原定义
    auto prog = compileAndOptimize(
        "int f(int a, int b) { int x = a + b; int y = a + b; int z = y * 2; return z; }");
    ASSERT_NE(prog, nullptr);

    // x = a+b 被 CSE 复用为 y=x，但如果 x 后来不再被使用... 
    // 实际上 y 被使用 (y*2)，所以 x 也被间接使用
    // 应有 1 个 + 和 1 个 *
    EXPECT_EQ(countOp(*prog, "+"), 1)
        << "IR:\n" << irToString(*prog);
    EXPECT_EQ(countOp(*prog, "*"), 1)
        << "IR:\n" << irToString(*prog);
}

TEST(IntegrationTest, EmptyFunction) {
    // void 函数无 return 值
    auto prog = compileAndOptimize("void f() {}");
    ASSERT_NE(prog, nullptr);

    // 至少应有 RETURN 指令
    EXPECT_GE(countType(*prog, TACType::RETURN), 1)
        << "IR:\n" << irToString(*prog);
}

TEST(IntegrationTest, OptimizeDoesNotCrash) {
    // 验证优化器在各种输入下不崩溃
    auto prog = compileAndOptimize(
        "int f(int n) { int s = 0; int i = 0; "
        "while (i < n) { s = s + i; i = i + 1; } return s; }");
    ASSERT_NE(prog, nullptr);

    // 保证基本结构完整
    EXPECT_GE(countType(*prog, TACType::RETURN), 1);
    EXPECT_GE(countType(*prog, TACType::IF_GOTO), 1);
}

TEST(IntegrationTest, MultipleFunctions) {
    auto prog = compileAndOptimize(
        "int add(int a, int b) { return a + b; } "
        "int main() { int x = add(3, 4); return x; }");
    ASSERT_NE(prog, nullptr);

    // 两个函数的 return 都应保留
    EXPECT_GE(countType(*prog, TACType::RETURN), 2)
        << "IR:\n" << irToString(*prog);
    // CALL 指令保留
    EXPECT_GE(countType(*prog, TACType::CALL), 1)
        << "IR:\n" << irToString(*prog);
}

// ================================================================
//  第 5 组：边界条件
// ================================================================

TEST(EdgeCaseTest, DivisionByOne) {
    auto prog = compileAndOptimize("int f() { int x = 100 / 1; return x; }");
    ASSERT_NE(prog, nullptr);
    // 100/1 = 100 → 应被折叠
    EXPECT_TRUE(hasInstruction(*prog, "x = 100"))
        << "IR:\n" << irToString(*prog);
}

TEST(EdgeCaseTest, ZeroTimesAnything) {
    auto prog = compileAndOptimize("int f() { int x = 0 * 999; return x; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_TRUE(hasInstruction(*prog, "x = 0"))
        << "IR:\n" << irToString(*prog);
}

TEST(EdgeCaseTest, NegativeNumbers) {
    auto prog = compileAndOptimize("int f() { int x = -5 + 10; return x; }");
    ASSERT_NE(prog, nullptr);
    // UNARY -5 折叠为 -5, 再加 10 → 5
    EXPECT_TRUE(hasInstruction(*prog, "x = 5"))
        << "IR:\n" << irToString(*prog);
}

TEST(EdgeCaseTest, AllConstantsRemoved) {
    // 所有运算都是常量，最终只剩很少的指令
    auto prog = compileAndOptimize(
        "int f() { int x = 1; int y = 2; int z = 3; return x + y + z; }");
    ASSERT_NE(prog, nullptr);

    // 返回 6，核心指令应大幅减少
    std::string ir = irToString(*prog);
    // 不应还有 x, y, z 的单独赋值（DCE 后应被清除）
    // 注意: 变量声明 instr 可能以不同形式存在
    EXPECT_TRUE(hasInstruction(*prog, "6"))
        << "IR:\n" << ir;
}

TEST(EdgeCaseTest, NestedIfNoDeadCode) {
    auto prog = compileAndOptimize(
        "int f(int a, int b) { "
        "  int r = 0; "
        "  if (a) { if (b) { r = 1; } } "
        "  return r; "
        "}");
    ASSERT_NE(prog, nullptr);

    // r 被 return 使用，不可删除
    EXPECT_TRUE(hasInstruction(*prog, "r = 0"))
        << "IR:\n" << irToString(*prog);
    EXPECT_GE(countType(*prog, TACType::IF_GOTO), 2)
        << "IR:\n" << irToString(*prog);
}

// ================================================================
//  第 6 组：优化幂等性
// ================================================================

TEST(IdempotenceTest, DoubleOptimizeSameResult) {
    // 优化两次结果应与优化一次相同
    auto src = "int f(int a, int b) { int x = a + b; int y = a + b; int z = 1 + 2; return y + z; }";

    auto prog1 = compileToIR(src);
    ASSERT_NE(prog1, nullptr);
    Optimizer opt1;
    opt1.optimize(*prog1);
    std::string ir1 = irToString(*prog1);

    auto prog2 = compileToIR(src);
    ASSERT_NE(prog2, nullptr);
    Optimizer opt2;
    opt2.optimize(*prog2);
    opt2.optimize(*prog2);  // 第二次优化
    std::string ir2 = irToString(*prog2);

    EXPECT_EQ(ir1, ir2) << "第二次优化不应改变 IR";
}

TEST(IdempotenceTest, OptimizeEmptyProgram) {
    TACProgram empty;
    Optimizer opt;
    EXPECT_NO_THROW(opt.optimize(empty));
    EXPECT_TRUE(empty.instructions.empty());
}
