#pragma once

#include "Token.h"
#include <string>
#include <vector>

namespace MyCompiler {

/// @brief 词法分析器：将字符流切割为 Token 流
class Lexer {
public:
    explicit Lexer(const std::string& source);  // 构造函数，接受源代码字符串，禁止隐式转换

    /// 扫描所有 Token（含 EOF）
    std::vector<Token> scanAll();

    /// 获取下一个 Token（不推进指针，用于 parser 向前看）
    Token peek();

    /// 获取当前 Token 并推进指针
    Token nextToken();

    /// 是否已到达末尾
    bool isAtEnd() const { return current_ >= source_.size(); }

private:
    std::string source_;    // 输入源代码，以字符串形式存储
    size_t      current_;   // 当前字符索引
    int         line_;      // 当前行号（从 1 开始）
    int         column_;    // 当前列号（从 1 开始）

    // 辅助方法
    char advance();                // 读取一个字符，推进指针
    char peekChar() const;         // 向前看一个字符（不推进）
    char peekNext() const;         // 向前看两个字符
    void skipWhitespace();         // 跳过空白字符和注释
    void skipLineComment();        // 跳过 // 单行注释
    void skipBlockComment();       // 跳过 /* */ 多行注释

    Token scanNumber();            // 扫描数字
    Token scanIdentifierOrKeyword(); // 扫描标识符/关键字
    Token scanOperator();          // 扫描运算符
};

} // namespace MyCompiler
