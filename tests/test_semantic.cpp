#include <gtest/gtest.h>
#include "../src/lexer/Lexer.h"
#include "../src/parser/Parser.h"
#include "../src/semantic/SemanticAnalyzer.h"

using namespace MyCompiler;

// ---- 辅助：解析 + 语义分析，返回错误数 ----
static int analyze(const std::string& src) {
    Lexer lexer(src);
    Parser parser(lexer);
    auto prog = parser.parse();
    SemanticAnalyzer sa;
    return sa.analyze(*prog);
}

// ================================================================
//  1. 正确程序 — 0 错误
// ================================================================

TEST(SemanticTest, VarDeclAndAssignment) {
    EXPECT_EQ(analyze("int main() { int x = 42; x = x + 1; return 0; }"), 0);
}

TEST(SemanticTest, BasicArithmetic) {
    EXPECT_EQ(analyze("int main() { int a = 10; int b = 20; int c = a + b; return c; }"), 0);
}

TEST(SemanticTest, IfWithIntCondition) {
    EXPECT_EQ(analyze("int main() { int x = 1; if (x) { x = 2; } return 0; }"), 0);
}

TEST(SemanticTest, IfElse) {
    EXPECT_EQ(analyze("int main() { int x = 0; if (1) { x = 1; } else { x = 2; } return x; }"), 0);
}

TEST(SemanticTest, WhileLoop) {
    EXPECT_EQ(analyze("int main() { int i = 0; while (i < 10) { i = i + 1; } return i; }"), 0);
}

TEST(SemanticTest, ComparisonReturnsInt) {
    EXPECT_EQ(analyze("int main() { int a = 5; int b = (a == 5); return b; }"), 0);
}

TEST(SemanticTest, NotOperator) {
    EXPECT_EQ(analyze("int main() { int a = 10; int b = !a; return b; }"), 0);
}

TEST(SemanticTest, UnaryMinus) {
    EXPECT_EQ(analyze("int main() { int x = 10; int y = -x; return y; }"), 0);
}

TEST(SemanticTest, UnaryPlus) {
    EXPECT_EQ(analyze("int main() { int x = 5; int y = +x; return y; }"), 0);
}

TEST(SemanticTest, LogicalAnd) {
    EXPECT_EQ(analyze("int main() { int a = 1; int b = 1; int c = a && b; return c; }"), 0);
}

TEST(SemanticTest, LogicalOr) {
    EXPECT_EQ(analyze("int main() { int a = 0; int b = 1; int c = a || b; return c; }"), 0);
}

TEST(SemanticTest, NestedScopes) {
    EXPECT_EQ(analyze("int main() { int a = 1; { int b = 2; a = a + b; } return a; }"), 0);
}

TEST(SemanticTest, VoidFunction) {
    EXPECT_EQ(analyze("void foo() { int x = 0; } int main() { foo(); return 0; }"), 0);
}

TEST(SemanticTest, FunctionCallWithArgs) {
    EXPECT_EQ(analyze(
        "int add(int a, int b) { return a + b; }"
        "int main() { return add(1, 2); }"), 0);
}

TEST(SemanticTest, FunctionCallNoArgs) {
    EXPECT_EQ(analyze(
        "int getNum() { return 42; }"
        "int main() { return getNum(); }"), 0);
}

TEST(SemanticTest, ConstDecl) {
    EXPECT_EQ(analyze("int main() { const int C = 100; return C; }"), 0);
}

TEST(SemanticTest, BreakInLoop) {
    EXPECT_EQ(analyze(
        "int main() { int i = 0; while (1) { if (i > 10) { break; } i = i + 1; } return i; }"), 0);
}

TEST(SemanticTest, ContinueInLoop) {
    EXPECT_EQ(analyze(
        "int main() { int i = 0; while (i < 10) { i = i + 1; continue; } return 0; }"), 0);
}

// ================================================================
//  2. 错误检测 — 应有 >0 错误
// ================================================================

TEST(SemanticTest, UndefinedVariable) {
    EXPECT_GT(analyze("int main() { x = 1; return 0; }"), 0);
}

TEST(SemanticTest, UndefinedVariableInExpr) {
    EXPECT_GT(analyze("int main() { int y = x + 1; return 0; }"), 0);
}

TEST(SemanticTest, DuplicateVarInSameScope) {
    EXPECT_GT(analyze("int main() { int x = 1; int x = 2; return 0; }"), 0);
}

TEST(SemanticTest, AssignToConst) {
    EXPECT_GT(analyze("int main() { const int C = 10; C = 20; return 0; }"), 0);
}

TEST(SemanticTest, BreakOutsideLoop) {
    EXPECT_GT(analyze("int main() { break; return 0; }"), 0);
}

TEST(SemanticTest, ContinueOutsideLoop) {
    EXPECT_GT(analyze("int main() { continue; return 0; }"), 0);
}

TEST(SemanticTest, NonIntCondition) {
    // 条件必须是 int, void 函数调用不能作条件
    // 这里测试: 如果用未定义的标识符作条件, 报错
    EXPECT_GT(analyze("int main() { if (undefined) { } return 0; }"), 0);
}

TEST(SemanticTest, MismatchedReturnType) {
    // return 空 但函数要求 int
    EXPECT_GT(analyze("int main() { return; }"), 0);
}

TEST(SemanticTest, TypeMismatchInAssignment) {
    // 赋值类型不匹配: void 函数返回值赋给 int
    EXPECT_GT(analyze(
        "void foo() { }"
        "int main() { int x = foo(); return 0; }"), 0);
}

// ================================================================
//  3. 边界测试
// ================================================================

TEST(SemanticTest, NestedWhileBreak) {
    EXPECT_EQ(analyze(
        "int main() {"
        "  int i = 0; int j = 0;"
        "  while (i < 5) {"
        "    while (j < 5) {"
        "      if (j > 2) { break; }"
        "      j = j + 1;"
        "    }"
        "    i = i + 1;"
        "  }"
        "  return i;"
        "}"), 0);
}

TEST(SemanticTest, ShadowedVariable) {
    // 内层屏蔽外层同名变量 — 应正确
    EXPECT_EQ(analyze("int main() { int x = 1; { int x = 2; } return x; }"), 0);
}
