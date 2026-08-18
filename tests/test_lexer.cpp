#include <gtest/gtest.h>
#include "../src/lexer/Lexer.h"
#include "../src/lexer/Token.h"
#include <vector>

using namespace MyCompiler;

// ---- 辅助：扫描源代码，去掉尾部 EOF ----
static std::vector<Token> scan(const std::string& src) {
    Lexer lexer(src);
    auto tokens = lexer.scanAll();
    if (!tokens.empty() && tokens.back().type == TokenType::EOF_TOKEN)
        tokens.pop_back();
    return tokens;
}

// ================================================================
//  1. 数字扫描
// ================================================================

TEST(LexerTest, DecimalNumber) {
    auto tokens = scan("123");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[0].lexeme, "123");
    EXPECT_EQ(tokens[0].intValue, 123);
}

TEST(LexerTest, Zero) {
    auto tokens = scan("0");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[0].intValue, 0);
}

TEST(LexerTest, HexNumber) {
    auto tokens = scan("0xFF");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[0].lexeme, "0xFF");
    EXPECT_EQ(tokens[0].intValue, 255);
}

TEST(LexerTest, BinaryNumber) {
    auto tokens = scan("0b1010");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[0].intValue, 10);
}

TEST(LexerTest, OctalNumber) {
    auto tokens = scan("077");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[0].intValue, 63);  // 7*8+7=63
}

// ================================================================
//  2. 关键字扫描
// ================================================================

TEST(LexerTest, KeywordIf) {
    auto tokens = scan("if");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::IF);
}

TEST(LexerTest, KeywordElse) {
    auto tokens = scan("else");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::ELSE);
}

TEST(LexerTest, KeywordWhile) {
    auto tokens = scan("while");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::WHILE);
}

TEST(LexerTest, KeywordReturn) {
    auto tokens = scan("return");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::RETURN);
}

TEST(LexerTest, KeywordInt) {
    auto tokens = scan("int");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::INT);
}

TEST(LexerTest, KeywordVoid) {
    auto tokens = scan("void");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::VOID);
}

TEST(LexerTest, KeywordConst) {
    auto tokens = scan("const");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::CONST);
}

TEST(LexerTest, KeywordBreakContinue) {
    auto tokens = scan("break continue");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, TokenType::BREAK);
    EXPECT_EQ(tokens[1].type, TokenType::CONTINUE);
}

// ================================================================
//  3. 标识符扫描
// ================================================================

TEST(LexerTest, SimpleIdentifier) {
    auto tokens = scan("foo");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].lexeme, "foo");
}

TEST(LexerTest, UnderscoreIdentifier) {
    auto tokens = scan("_test");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].lexeme, "_test");
}

TEST(LexerTest, AlphanumericIdentifier) {
    auto tokens = scan("x1 var2 foo_bar");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].lexeme, "x1");
    EXPECT_EQ(tokens[1].lexeme, "var2");
    EXPECT_EQ(tokens[2].lexeme, "foo_bar");
}

// ================================================================
//  4. 运算符扫描
// ================================================================

TEST(LexerTest, ArithmeticOperators) {
    auto tokens = scan("+ - * / %");
    ASSERT_EQ(tokens.size(), 5u);
    EXPECT_EQ(tokens[0].type, TokenType::PLUS);
    EXPECT_EQ(tokens[1].type, TokenType::MINUS);
    EXPECT_EQ(tokens[2].type, TokenType::STAR);
    EXPECT_EQ(tokens[3].type, TokenType::SLASH);
    EXPECT_EQ(tokens[4].type, TokenType::MOD);
}

TEST(LexerTest, RelationalOperators) {
    auto tokens = scan("< > <= >= == !=");
    ASSERT_EQ(tokens.size(), 6u);
    EXPECT_EQ(tokens[0].type, TokenType::LT);
    EXPECT_EQ(tokens[1].type, TokenType::GT);
    EXPECT_EQ(tokens[2].type, TokenType::LE);
    EXPECT_EQ(tokens[3].type, TokenType::GE);
    EXPECT_EQ(tokens[4].type, TokenType::EQ);
    EXPECT_EQ(tokens[5].type, TokenType::NEQ);
}

TEST(LexerTest, LogicalOperators) {
    auto tokens = scan("! && ||");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].type, TokenType::NOT);
    EXPECT_EQ(tokens[1].type, TokenType::AND);
    EXPECT_EQ(tokens[2].type, TokenType::OR);
}

TEST(LexerTest, AssignOperator) {
    auto tokens = scan("x = 5");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[1].type, TokenType::ASSIGN);
    EXPECT_EQ(tokens[2].type, TokenType::NUMBER);
}

// ================================================================
//  5. 分隔符扫描
// ================================================================

TEST(LexerTest, Delimiters) {
    auto tokens = scan("( ) { } ; ,");
    ASSERT_EQ(tokens.size(), 6u);
    EXPECT_EQ(tokens[0].type, TokenType::LPAREN);
    EXPECT_EQ(tokens[1].type, TokenType::RPAREN);
    EXPECT_EQ(tokens[2].type, TokenType::LBRACE);
    EXPECT_EQ(tokens[3].type, TokenType::RBRACE);
    EXPECT_EQ(tokens[4].type, TokenType::SEMICOLON);
    EXPECT_EQ(tokens[5].type, TokenType::COMMA);
}

// ================================================================
//  6. 注释扫描（应被忽略）
// ================================================================

TEST(LexerTest, LineComment) {
    auto tokens = scan("// this is a comment\n42");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[0].intValue, 42);
}

TEST(LexerTest, BlockComment) {
    auto tokens = scan("/* block comment */ 100");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[0].intValue, 100);
}

TEST(LexerTest, MixedComment) {
    auto tokens = scan("/* multi\nline */ // single\n7");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].intValue, 7);
}

// ================================================================
//  7. 综合测试
// ================================================================

TEST(LexerTest, FullStatement) {
    auto tokens = scan("int x = 42;");
    ASSERT_EQ(tokens.size(), 5u);
    EXPECT_EQ(tokens[0].type, TokenType::INT);
    EXPECT_EQ(tokens[1].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[1].lexeme, "x");
    EXPECT_EQ(tokens[2].type, TokenType::ASSIGN);
    EXPECT_EQ(tokens[3].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[3].intValue, 42);
    EXPECT_EQ(tokens[4].type, TokenType::SEMICOLON);
}

TEST(LexerTest, IfStatement) {
    auto tokens = scan("if (x > 0) { return x; }");
    ASSERT_EQ(tokens.size(), 11u);
    EXPECT_EQ(tokens[0].type, TokenType::IF);
    EXPECT_EQ(tokens[1].type, TokenType::LPAREN);
    EXPECT_EQ(tokens[2].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[3].type, TokenType::GT);
    EXPECT_EQ(tokens[4].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[5].type, TokenType::RPAREN);
    EXPECT_EQ(tokens[6].type, TokenType::LBRACE);
    EXPECT_EQ(tokens[7].type, TokenType::RETURN);
    EXPECT_EQ(tokens[8].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[9].type, TokenType::SEMICOLON);
    EXPECT_EQ(tokens[10].type, TokenType::RBRACE);
}
