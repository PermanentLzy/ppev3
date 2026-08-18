#include <gtest/gtest.h>
#include "../src/ast/AST.h"
#include "../src/lexer/Token.h"

using namespace MyCompiler;

// ================================================================
//  AST 节点构造测试 — 验证节点可以正确创建
//  实际的树形打印功能 (ASTPrinter) 可通过 DEBUG_AST=1 环境变量测试
// ================================================================

// ---- 表达式节点 ----

TEST(ASTTest, NumberLiteralNode) {
    auto n = std::make_unique<NumberLiteral>(42);
    EXPECT_EQ(n->value, 42);
}

TEST(ASTTest, IdentifierExprNode) {
    auto n = std::make_unique<IdentifierExpr>("myVar");
    EXPECT_EQ(n->name, "myVar");
}

TEST(ASTTest, BinaryExprNode) {
    auto left = std::make_unique<NumberLiteral>(1);
    auto right = std::make_unique<NumberLiteral>(2);
    auto bin = std::make_unique<BinaryExpr>(
        std::move(left), TokenType::PLUS, std::move(right));
    EXPECT_EQ(bin->op, TokenType::PLUS);
    EXPECT_NE(bin->left, nullptr);
    EXPECT_NE(bin->right, nullptr);
}

TEST(ASTTest, UnaryExprNode) {
    auto operand = std::make_unique<NumberLiteral>(5);
    auto un = std::make_unique<UnaryExpr>(TokenType::MINUS, std::move(operand));
    EXPECT_EQ(un->op, TokenType::MINUS);
    EXPECT_NE(un->operand, nullptr);
}

TEST(ASTTest, AssignExprNode) {
    auto val = std::make_unique<NumberLiteral>(10);
    auto assign = std::make_unique<AssignExpr>("x", std::move(val));
    EXPECT_EQ(assign->name, "x");
    EXPECT_NE(assign->value, nullptr);
}

TEST(ASTTest, CallExprNode) {
    std::vector<std::unique_ptr<Expr>> args;
    args.push_back(std::make_unique<NumberLiteral>(1));
    args.push_back(std::make_unique<NumberLiteral>(2));
    auto call = std::make_unique<CallExpr>("foo", std::move(args));
    EXPECT_EQ(call->funcName, "foo");
    EXPECT_EQ(call->args.size(), 2u);
}

// ---- 语句节点 ----

TEST(ASTTest, ExprStmtNode) {
    auto es = std::make_unique<ExprStmt>(std::make_unique<NumberLiteral>(1));
    EXPECT_NE(es->expr, nullptr);
}

TEST(ASTTest, VarDeclStmtNode) {
    auto init = std::make_unique<NumberLiteral>(42);
    auto vd = std::make_unique<VarDeclStmt>(DataType::INT, "x", std::move(init), false);
    EXPECT_EQ(vd->type, DataType::INT);
    EXPECT_EQ(vd->name, "x");
    EXPECT_FALSE(vd->isConst);
}

TEST(ASTTest, ConstDeclStmtNode) {
    auto init = std::make_unique<NumberLiteral>(100);
    auto vd = std::make_unique<VarDeclStmt>(DataType::INT, "C", std::move(init), true);
    EXPECT_TRUE(vd->isConst);
}

TEST(ASTTest, IfStmtNode) {
    auto cond = std::make_unique<IdentifierExpr>("flag");
    auto thenB = std::make_unique<ExprStmt>(std::make_unique<NumberLiteral>(0));
    auto ifStmt = std::make_unique<IfStmt>(std::move(cond), std::move(thenB), nullptr);
    EXPECT_NE(ifStmt->condition, nullptr);
    EXPECT_NE(ifStmt->thenBranch, nullptr);
    EXPECT_EQ(ifStmt->elseBranch, nullptr);
}

TEST(ASTTest, WhileStmtNode) {
    auto cond = std::make_unique<IdentifierExpr>("x");
    auto body = std::make_unique<BreakStmt>();
    auto wh = std::make_unique<WhileStmt>(std::move(cond), std::move(body));
    EXPECT_NE(wh->condition, nullptr);
    EXPECT_NE(wh->body, nullptr);
}

TEST(ASTTest, BreakContinueReturnNodes) {
    auto br = std::make_unique<BreakStmt>();
    auto ct = std::make_unique<ContinueStmt>();
    auto ret1 = std::make_unique<ReturnStmt>(std::make_unique<NumberLiteral>(0));
    auto ret2 = std::make_unique<ReturnStmt>(nullptr);
    EXPECT_EQ(ret1->value != nullptr, true);
    EXPECT_EQ(ret2->value, nullptr);
}

TEST(ASTTest, BlockStmtNode) {
    auto block = std::make_unique<BlockStmt>();
    block->statements.push_back(std::make_unique<BreakStmt>());
    block->statements.push_back(std::make_unique<ContinueStmt>());
    EXPECT_EQ(block->statements.size(), 2u);
}

TEST(ASTTest, FunctionDeclNode) {
    std::vector<std::pair<std::string, DataType>> params;
    params.push_back({"a", DataType::INT});
    auto body = std::make_unique<BlockStmt>();
    auto fn = std::make_unique<FunctionDecl>(DataType::INT, "main", std::move(params), std::move(body));
    EXPECT_EQ(fn->returnType, DataType::INT);
    EXPECT_EQ(fn->name, "main");
    EXPECT_EQ(fn->params.size(), 1u);
}

TEST(ASTTest, ProgramNode) {
    auto prog = std::make_unique<Program>();
    prog->statements.push_back(
        std::make_unique<VarDeclStmt>(DataType::INT, "g", std::make_unique<NumberLiteral>(0), false));
    EXPECT_EQ(prog->statements.size(), 1u);
    EXPECT_TRUE(prog->functions.empty());
}
