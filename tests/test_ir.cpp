#include <gtest/gtest.h>
#include "../src/lexer/Lexer.h"
#include "../src/parser/Parser.h"
#include "../src/parser/ParseError.h"
#include "../src/ir/IRGenerator.h"
#include "../src/ir/ThreeAddrCode.h"
#include <sstream>

using namespace MyCompiler;

// ---- 辅助：完整编译管线（源码 → AST → IR）----
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

/// 辅助：断言 IR 中某条指令符合预期
static void expectInstr(const TACInstruction& instr,
                        TACType type,
                        const std::string& result = "",
                        const std::string& lhs = "",
                        const std::string& rhs = "",
                        const std::string& op = "",
                        const std::string& label = "") {
    EXPECT_EQ(instr.type, type);
    if (!result.empty()) EXPECT_EQ(instr.result.toString(), result);
    if (!lhs.empty())    EXPECT_EQ(instr.lhs.toString(), lhs);
    if (!rhs.empty())    EXPECT_EQ(instr.rhs.toString(), rhs);
    if (!op.empty())     EXPECT_EQ(instr.op, op);
    if (!label.empty())  EXPECT_EQ(instr.label, label);
}

/// 辅助：统计某种指令的出现次数
static int countType(const TACProgram& prog, TACType type) {
    int cnt = 0;
    for (auto& i : prog.instructions)
        if (i.type == type) ++cnt;
    return cnt;
}

// ================================================================
//  1. 字面量 & 标识符
// ================================================================

TEST(IRGenTest, NumberLiteral) {
    auto prog = compileToIR("int f() { int x = 42; }");
    ASSERT_NE(prog, nullptr);
    // 应该包含: ASSIGN x = 42
    EXPECT_GE(countType(*prog, TACType::ASSIGN), 1);
    // 检查 toString() 不崩溃
    std::ostringstream oss;
    for (auto& i : prog->instructions) {
        oss << i.toString() << "\n";
    }
    EXPECT_FALSE(oss.str().empty());
}

TEST(IRGenTest, IdentifierExpr) {
    auto prog = compileToIR("int f() { int a = 1; int b = a; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_GE(countType(*prog, TACType::ASSIGN), 2);
}

// ================================================================
//  2. 二元运算（算术 & 比较）
// ================================================================

TEST(IRGenTest, SimpleAddition) {
    auto prog = compileToIR("int f() { int x = 1 + 2; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_GE(countType(*prog, TACType::BINARY), 1);
    // 验证有 + 运算符
    bool foundPlus = false;
    for (auto& i : prog->instructions) {
        if (i.type == TACType::BINARY && i.op == "+") {
            foundPlus = true;
            break;
        }
    }
    EXPECT_TRUE(foundPlus);
}

TEST(IRGenTest, Multiplication) {
    auto prog = compileToIR("int f() { int x = 3 * 4; }");
    ASSERT_NE(prog, nullptr);
    bool foundMul = false;
    for (auto& i : prog->instructions) {
        if (i.type == TACType::BINARY && i.op == "*") {
            foundMul = true;
            break;
        }
    }
    EXPECT_TRUE(foundMul);
}

TEST(IRGenTest, ComparisonLT) {
    auto prog = compileToIR("int f() { int x = a < b; }");
    ASSERT_NE(prog, nullptr);
    bool found = false;
    for (auto& i : prog->instructions) {
        if (i.type == TACType::BINARY && i.op == "<") {
            found = true; break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(IRGenTest, ComparisonEQ) {
    auto prog = compileToIR("int f() { int x = a == b; }");
    ASSERT_NE(prog, nullptr);
    bool found = false;
    for (auto& i : prog->instructions) {
        if (i.type == TACType::BINARY && i.op == "==") {
            found = true; break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(IRGenTest, ComplexArithmetic) {
    // 1 + 2 * 3
    auto prog = compileToIR("int f() { int x = 1 + 2 * 3; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_GE(countType(*prog, TACType::BINARY), 2);
}

// ================================================================
//  3. 一元运算
// ================================================================

TEST(IRGenTest, UnaryMinus) {
    auto prog = compileToIR("int f() { int x = -42; }");
    ASSERT_NE(prog, nullptr);
    bool found = false;
    for (auto& i : prog->instructions) {
        if (i.type == TACType::UNARY && i.op == "-") {
            found = true; break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(IRGenTest, UnaryNot) {
    auto prog = compileToIR("int f() { int x = !a; }");
    ASSERT_NE(prog, nullptr);
    bool found = false;
    for (auto& i : prog->instructions) {
        if (i.type == TACType::UNARY && i.op == "!") {
            found = true; break;
        }
    }
    EXPECT_TRUE(found);
}

// ================================================================
//  4. 短路求值 (&& / ||)
// ================================================================

TEST(IRGenTest, LogicalAndShortCircuit) {
    auto prog = compileToIR("int f() { int x = a && b; }");
    ASSERT_NE(prog, nullptr);
    // && 使用 IF_GOTO 实现短路，不应有 BINARY && 指令
    EXPECT_GE(countType(*prog, TACType::IF_GOTO), 1);
    EXPECT_GE(countType(*prog, TACType::LABEL), 2);
}

TEST(IRGenTest, LogicalOrShortCircuit) {
    auto prog = compileToIR("int f() { int x = a || b; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_GE(countType(*prog, TACType::IF_GOTO), 1);
    EXPECT_GE(countType(*prog, TACType::LABEL), 2);
}

// ================================================================
//  5. 赋值语句
// ================================================================

TEST(IRGenTest, SimpleAssignment) {
    auto prog = compileToIR("int f() { int x = 0; x = 10; }");
    ASSERT_NE(prog, nullptr);
    // 应有至少 2 条 ASSIGN: x=0 和 x=10
    EXPECT_GE(countType(*prog, TACType::ASSIGN), 2);
}

// ================================================================
//  6. 控制流：if / if-else
// ================================================================

TEST(IRGenTest, IfStmt) {
    auto prog = compileToIR("int f() { int x = 0; if (x) { x = 1; } }");
    ASSERT_NE(prog, nullptr);
    // 应有 IF_GOTO 和 LABEL
    EXPECT_GE(countType(*prog, TACType::IF_GOTO), 1);
    EXPECT_GE(countType(*prog, TACType::LABEL), 2); // L_then + L_end
    EXPECT_GE(countType(*prog, TACType::GOTO), 1);
}

TEST(IRGenTest, IfElseStmt) {
    auto prog = compileToIR("int f() { int x = 0; if (x) { x = 1; } else { x = 2; } }");
    ASSERT_NE(prog, nullptr);
    EXPECT_GE(countType(*prog, TACType::IF_GOTO), 1);
    EXPECT_GE(countType(*prog, TACType::LABEL), 3); // L_then + L_else + L_end
    EXPECT_GE(countType(*prog, TACType::GOTO), 2);
}

// ================================================================
//  7. 控制流：while
// ================================================================

TEST(IRGenTest, WhileLoop) {
    auto prog = compileToIR("int f() { int x = 5; while (x) { x = x - 1; } }");
    ASSERT_NE(prog, nullptr);
    EXPECT_GE(countType(*prog, TACType::IF_GOTO), 1);
    EXPECT_GE(countType(*prog, TACType::LABEL), 3); // L_start + L_body + L_end
    EXPECT_GE(countType(*prog, TACType::GOTO), 2);  // goto L_end + goto L_start
}

// ================================================================
//  8. break / continue
// ================================================================

TEST(IRGenTest, BreakInLoop) {
    // while (1) { break; }
    auto prog = compileToIR("int f() { while (1) { break; } }");
    ASSERT_NE(prog, nullptr);
    EXPECT_GE(countType(*prog, TACType::GOTO), 2); // 其中一个 goto 是 break 生成的
}

TEST(IRGenTest, ContinueInLoop) {
    auto prog = compileToIR("int f() { int x = 0; while (x) { x = x - 1; continue; } }");
    ASSERT_NE(prog, nullptr);
    EXPECT_GE(countType(*prog, TACType::GOTO), 3); // break, continue, loop back
}

// ================================================================
//  9. return 语句
// ================================================================

TEST(IRGenTest, ReturnWithValue) {
    auto prog = compileToIR("int f() { return 42; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_GE(countType(*prog, TACType::RETURN), 1);
}

TEST(IRGenTest, ReturnVoid) {
    auto prog = compileToIR("void f() { return; }");
    ASSERT_NE(prog, nullptr);
    EXPECT_GE(countType(*prog, TACType::RETURN), 1);
}

// ================================================================
//  10. 函数声明
// ================================================================

TEST(IRGenTest, FunctionDeclHasLabel) {
    auto prog = compileToIR("int add(int a, int b) { return a + b; }");
    ASSERT_NE(prog, nullptr);
    bool hasFuncLabel = false;
    for (auto& i : prog->instructions) {
        if (i.type == TACType::LABEL && i.label.find("func_") != std::string::npos) {
            hasFuncLabel = true;
            break;
        }
    }
    EXPECT_TRUE(hasFuncLabel);
}

TEST(IRGenTest, FunctionDeclHasReturn) {
    auto prog = compileToIR("int f() { int x = 1; }");
    ASSERT_NE(prog, nullptr);
    // int 函数末尾应自动补 return 0
    EXPECT_GE(countType(*prog, TACType::RETURN), 1);
}

// ================================================================
//  11. 完整程序（Program 节点）
// ================================================================

TEST(IRGenTest, ProgramHasCallMain) {
    auto prog = compileToIR("int main() { return 0; }");
    ASSERT_NE(prog, nullptr);
    bool hasCallMain = false;
    for (auto& i : prog->instructions) {
        if (i.type == TACType::CALL && i.label == "main") {
            hasCallMain = true;
            break;
        }
    }
    EXPECT_TRUE(hasCallMain);
}

TEST(IRGenTest, GlobalVarDecl) {
    auto prog = compileToIR("int x = 10; int main() { return x; }");
    ASSERT_NE(prog, nullptr);
    // 应有全局变量声明生成的 ASSIGN
    EXPECT_GE(countType(*prog, TACType::ASSIGN), 1);
}

// ================================================================
//  12. 三地址码打印（不崩溃即可）
// ================================================================

TEST(IRGenTest, PrintDoesNotCrash) {
    auto prog = compileToIR(
        "int main() {\n"
        "    int x = 10;\n"
        "    int y = x + 5;\n"
        "    if (x > 0) {\n"
        "        y = y - 1;\n"
        "    }\n"
        "    return y;\n"
        "}\n");
    ASSERT_NE(prog, nullptr);
    // print 不应崩溃
    testing::internal::CaptureStdout();
    prog->print();
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_FALSE(output.empty());
    // 验证包含基本的三地址码格式
    EXPECT_TRUE(output.find("func_main") != std::string::npos ||
                output.find("main") != std::string::npos);
}

// ================================================================
//  13. 边界情况
// ================================================================

TEST(IRGenTest, EmptyProgram) {
    auto prog = compileToIR("");
    ASSERT_NE(prog, nullptr);
    // 空程序至少应有: GOTO L_main_call, LABEL L_main_call, CALL main, RETURN
    EXPECT_GE(prog->instructions.size(), 4u);
}

TEST(IRGenTest, NestedIfElse) {
    auto prog = compileToIR(
        "int f() {\n"
        "    int x = 0;\n"
        "    if (x) {\n"
        "        if (x > 5) { x = 1; }\n"
        "    }\n"
        "}\n");
    ASSERT_NE(prog, nullptr);
    EXPECT_GE(countType(*prog, TACType::IF_GOTO), 2);
}
