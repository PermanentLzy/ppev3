#include <gtest/gtest.h>
#include "../src/lexer/Lexer.h"
#include "../src/parser/Parser.h"
#include "../src/parser/ParseError.h"
#include "../src/ast/AST.h"
#include <memory>

using namespace MyCompiler;

// ---- 辅助：解析源代码 ----
static std::unique_ptr<Program> parse(const std::string& src) {
    Lexer lexer(src);
    Parser parser(lexer);
    return parser.parse();
}

// ================================================================
//  1. 字面量与标识符表达式
// ================================================================

TEST(ParserTest, NumberLiteral) {
    auto prog = parse("int x = 42;");
    ASSERT_EQ(prog->statements.size(), 1u);
    auto* vd = dynamic_cast<VarDeclStmt*>(prog->statements[0].get());
    ASSERT_NE(vd, nullptr);
    auto* num = dynamic_cast<NumberLiteral*>(vd->init.get());
    ASSERT_NE(num, nullptr);
    EXPECT_EQ(num->value, 42);
}

TEST(ParserTest, IdentifierExpr) {
    // 顶层只能是声明或函数, 用局部声明测试
    auto prog = parse("int f() { int x = 1; int y = x; }");
    ASSERT_EQ(prog->functions.size(), 1u);
    auto* fn = prog->functions[0].get();
    EXPECT_EQ(fn->name, "f");
}

// ================================================================
//  2. 二元表达式（运算符优先级）
// ================================================================

TEST(ParserTest, SimpleAddition) {
    auto prog = parse("int x = 1 + 2;");
    auto* vd = dynamic_cast<VarDeclStmt*>(prog->statements[0].get());
    auto* bin = dynamic_cast<BinaryExpr*>(vd->init.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->op, TokenType::PLUS);
}

TEST(ParserTest, OperatorPrecedence) {
    // 1 + 2 * 3 → 乘法优先
    auto prog = parse("int x = 1 + 2 * 3;");
    auto* vd = dynamic_cast<VarDeclStmt*>(prog->statements[0].get());
    auto* bin = dynamic_cast<BinaryExpr*>(vd->init.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->op, TokenType::PLUS);
    auto* right = dynamic_cast<BinaryExpr*>(bin->right.get());
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(right->op, TokenType::STAR);
}

TEST(ParserTest, ParenthesizedExpr) {
    // (1 + 2) * 3 → 括号优先
    auto prog = parse("int x = (1 + 2) * 3;");
    auto* vd = dynamic_cast<VarDeclStmt*>(prog->statements[0].get());
    auto* bin = dynamic_cast<BinaryExpr*>(vd->init.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->op, TokenType::STAR);
    auto* left = dynamic_cast<BinaryExpr*>(bin->left.get());
    ASSERT_NE(left, nullptr);
    EXPECT_EQ(left->op, TokenType::PLUS);
}

TEST(ParserTest, Comparison) {
    auto prog = parse("int x = a < b;");
    auto* vd = dynamic_cast<VarDeclStmt*>(prog->statements[0].get());
    auto* bin = dynamic_cast<BinaryExpr*>(vd->init.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->op, TokenType::LT);
}

TEST(ParserTest, Equality) {
    auto prog = parse("int x = a == b;");
    auto* vd = dynamic_cast<VarDeclStmt*>(prog->statements[0].get());
    auto* bin = dynamic_cast<BinaryExpr*>(vd->init.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->op, TokenType::EQ);
}

TEST(ParserTest, LogicalAnd) {
    auto prog = parse("int x = a && b;");
    auto* vd = dynamic_cast<VarDeclStmt*>(prog->statements[0].get());
    auto* bin = dynamic_cast<BinaryExpr*>(vd->init.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->op, TokenType::AND);
}

TEST(ParserTest, LogicalOr) {
    auto prog = parse("int x = a || b;");
    auto* vd = dynamic_cast<VarDeclStmt*>(prog->statements[0].get());
    auto* bin = dynamic_cast<BinaryExpr*>(vd->init.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->op, TokenType::OR);
}

TEST(ParserTest, Modulo) {
    auto prog = parse("int x = 10 % 3;");
    auto* vd = dynamic_cast<VarDeclStmt*>(prog->statements[0].get());
    auto* bin = dynamic_cast<BinaryExpr*>(vd->init.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->op, TokenType::MOD);
}

// ================================================================
//  3. 一元表达式
// ================================================================

TEST(ParserTest, UnaryMinus) {
    auto prog = parse("int x = -42;");
    auto* vd = dynamic_cast<VarDeclStmt*>(prog->statements[0].get());
    auto* un = dynamic_cast<UnaryExpr*>(vd->init.get());
    ASSERT_NE(un, nullptr);
    EXPECT_EQ(un->op, TokenType::MINUS);
}

TEST(ParserTest, UnaryNot) {
    auto prog = parse("int x = !a;");
    auto* vd = dynamic_cast<VarDeclStmt*>(prog->statements[0].get());
    auto* un = dynamic_cast<UnaryExpr*>(vd->init.get());
    ASSERT_NE(un, nullptr);
    EXPECT_EQ(un->op, TokenType::NOT);
}

TEST(ParserTest, UnaryPlus) {
    auto prog = parse("int x = +5;");
    auto* vd = dynamic_cast<VarDeclStmt*>(prog->statements[0].get());
    auto* un = dynamic_cast<UnaryExpr*>(vd->init.get());
    ASSERT_NE(un, nullptr);
    EXPECT_EQ(un->op, TokenType::PLUS);
}

// ================================================================
//  4. 语句解析
// ================================================================

TEST(ParserTest, EmptyStatement) {
    auto prog = parse("int f() { ; }");
    ASSERT_EQ(prog->functions.size(), 1u);
    auto* body = prog->functions[0]->body.get();
    ASSERT_EQ(body->statements.size(), 1u);
    auto* es = dynamic_cast<ExprStmt*>(body->statements[0].get());
    ASSERT_NE(es, nullptr);
    EXPECT_EQ(es->expr, nullptr);
}

TEST(ParserTest, Assignment) {
    auto prog = parse("int f() { int x = 0; x = 10; }");
    auto* body = prog->functions[0]->body.get();
    ASSERT_EQ(body->statements.size(), 2u);
}

TEST(ParserTest, IfStmt) {
    auto prog = parse("int f() { int x = 0; if (x) { x = 1; } }");
    auto* body = prog->functions[0]->body.get();
    ASSERT_EQ(body->statements.size(), 2u);
    auto* ifStmt = dynamic_cast<IfStmt*>(body->statements[1].get());
    ASSERT_NE(ifStmt, nullptr);
    EXPECT_EQ(ifStmt->elseBranch, nullptr);
}

TEST(ParserTest, IfElseStmt) {
    auto prog = parse("int f() { int x = 0; if (x) { x = 1; } else { x = 2; } }");
    auto* body = prog->functions[0]->body.get();
    ASSERT_EQ(body->statements.size(), 2u);
    auto* ifStmt = dynamic_cast<IfStmt*>(body->statements[1].get());
    ASSERT_NE(ifStmt, nullptr);
    EXPECT_NE(ifStmt->elseBranch, nullptr);
}

TEST(ParserTest, WhileStmt) {
    auto prog = parse("int f() { int x = 0; while (x) { x = x - 1; } }");
    auto* body = prog->functions[0]->body.get();
    ASSERT_EQ(body->statements.size(), 2u);
    auto* wh = dynamic_cast<WhileStmt*>(body->statements[1].get());
    ASSERT_NE(wh, nullptr);
}

TEST(ParserTest, BreakStmt) {
    auto prog = parse("int f() { while (1) { break; } }");
    auto* body = prog->functions[0]->body.get();
    auto* wh = dynamic_cast<WhileStmt*>(body->statements[0].get());
    ASSERT_NE(wh, nullptr);
    auto* whBody = dynamic_cast<BlockStmt*>(wh->body.get());
    ASSERT_NE(whBody, nullptr);
    auto* br = dynamic_cast<BreakStmt*>(whBody->statements[0].get());
    ASSERT_NE(br, nullptr);
}

TEST(ParserTest, ContinueStmt) {
    auto prog = parse("int f() { while (1) { continue; } }");
    auto* body = prog->functions[0]->body.get();
    auto* wh = dynamic_cast<WhileStmt*>(body->statements[0].get());
    ASSERT_NE(wh, nullptr);
    auto* whBody = dynamic_cast<BlockStmt*>(wh->body.get());
    ASSERT_NE(whBody, nullptr);
    auto* ct = dynamic_cast<ContinueStmt*>(whBody->statements[0].get());
    ASSERT_NE(ct, nullptr);
}

TEST(ParserTest, ReturnStmt) {
    auto prog = parse("int f() { return 0; }");
    auto* body = prog->functions[0]->body.get();
    auto* ret = dynamic_cast<ReturnStmt*>(body->statements[0].get());
    ASSERT_NE(ret, nullptr);
    EXPECT_NE(ret->value, nullptr);
}

TEST(ParserTest, ReturnVoid) {
    auto prog = parse("void f() { return; }");
    auto* body = prog->functions[0]->body.get();
    auto* ret = dynamic_cast<ReturnStmt*>(body->statements[0].get());
    ASSERT_NE(ret, nullptr);
    EXPECT_EQ(ret->value, nullptr);
}

// ================================================================
//  5. 声明解析
// ================================================================

TEST(ParserTest, VarDecl) {
    auto prog = parse("int x = 42;");
    auto* vd = dynamic_cast<VarDeclStmt*>(prog->statements[0].get());
    ASSERT_NE(vd, nullptr);
    EXPECT_EQ(vd->type, DataType::INT);
    EXPECT_EQ(vd->name, "x");
    EXPECT_FALSE(vd->isConst);
}

TEST(ParserTest, ConstDecl) {
    auto prog = parse("const int x = 42;");
    auto* vd = dynamic_cast<VarDeclStmt*>(prog->statements[0].get());
    ASSERT_NE(vd, nullptr);
    EXPECT_EQ(vd->type, DataType::INT);
    EXPECT_EQ(vd->name, "x");
    EXPECT_TRUE(vd->isConst);
}

TEST(ParserTest, DeclMustInit) {
    // ToyC 要求声明必须初始化, 缺少 = 应抛出 ParseError
    EXPECT_THROW(parse("int x;"), ParseError);
}

// ================================================================
//  6. 函数定义解析
// ================================================================

TEST(ParserTest, FuncDefIntReturn) {
    auto prog = parse("int foo() { return 42; }");
    ASSERT_EQ(prog->functions.size(), 1u);
    auto* fn = prog->functions[0].get();
    EXPECT_EQ(fn->returnType, DataType::INT);
    EXPECT_EQ(fn->name, "foo");
    EXPECT_TRUE(fn->params.empty());
}

TEST(ParserTest, FuncDefVoidReturn) {
    auto prog = parse("void bar() { }");
    ASSERT_EQ(prog->functions.size(), 1u);
    auto* fn = prog->functions[0].get();
    EXPECT_EQ(fn->returnType, DataType::VOID);
    EXPECT_EQ(fn->name, "bar");
}

TEST(ParserTest, FuncDefWithParams) {
    auto prog = parse("int add(int a, int b) { return a + b; }");
    ASSERT_EQ(prog->functions.size(), 1u);
    auto* fn = prog->functions[0].get();
    EXPECT_EQ(fn->params.size(), 2u);
    EXPECT_EQ(fn->params[0].first, "a");
    EXPECT_EQ(fn->params[0].second, DataType::INT);
    EXPECT_EQ(fn->params[1].first, "b");
    EXPECT_EQ(fn->params[1].second, DataType::INT);
}

TEST(ParserTest, MixedDeclAndFunc) {
    auto prog = parse("int global = 10; int main() { return global; }");
    ASSERT_EQ(prog->statements.size(), 1u);
    ASSERT_EQ(prog->functions.size(), 1u);
}

// ================================================================
//  7. 函数调用表达式
// ================================================================

TEST(ParserTest, FunctionCall) {
    auto prog = parse("int f() { return foo(1, 2); }");
    auto* body = prog->functions[0]->body.get();
    auto* ret = dynamic_cast<ReturnStmt*>(body->statements[0].get());
    auto* call = dynamic_cast<CallExpr*>(ret->value.get());
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->funcName, "foo");
    EXPECT_EQ(call->args.size(), 2u);
}

TEST(ParserTest, FunctionCallNoArgs) {
    auto prog = parse("int f() { return bar(); }");
    auto* body = prog->functions[0]->body.get();
    auto* ret = dynamic_cast<ReturnStmt*>(body->statements[0].get());
    auto* call = dynamic_cast<CallExpr*>(ret->value.get());
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->funcName, "bar");
    EXPECT_TRUE(call->args.empty());
}
