#pragma once

#include <string>
#include <unordered_map>

namespace MyCompiler {

/// @brief Token 类型枚举
/// @note  ToyC 只有 int/void 类型，无条件表达式用 int（非零为真）
enum class TokenType {
    // 关键字
    IF, ELSE, WHILE, RETURN, INT, VOID,
    BREAK, CONTINUE, CONST,

    // 字面量
    NUMBER,       // 整数（含负数，如 -42）
    IDENTIFIER,   // 标识符

    // 运算符
    PLUS,         // +
    MINUS,        // -
    STAR,         // *
    SLASH,        // /
    MOD,          // %
    ASSIGN,       // =
    EQ,           // ==
    NEQ,          // !=
    LT,           // <
    LE,           // <=
    GT,           // >
    GE,           // >=
    NOT,          // !
    AND,          // &&
    OR,           // ||

    // 分隔符
    LPAREN,       // (
    RPAREN,       // )
    LBRACE,       // {
    RBRACE,       // }
    SEMICOLON,    // ;
    COMMA,        // ,

    // 特殊
    EOF_TOKEN,    // 文件结束
    UNKNOWN       // 未知字符
};

/// @brief 关键字字符串 -> TokenType 映射
/// @note  ToyC 没有 bool/true/false 关键字
inline const std::unordered_map<std::string, TokenType>& keywordMap() {
    static const std::unordered_map<std::string, TokenType> map = {
        {"if",       TokenType::IF},
        {"else",     TokenType::ELSE},
        {"while",    TokenType::WHILE},
        {"return",   TokenType::RETURN},
        {"int",      TokenType::INT},
        {"void",     TokenType::VOID},
        {"break",    TokenType::BREAK},
        {"continue", TokenType::CONTINUE},
        {"const",    TokenType::CONST},
    };
    return map;
}

/// @brief Token 结构体
struct Token {
    TokenType type;           //Token 类型
    std::string lexeme;       //词素文本（如 "if", "123", "variableName"）
    int line;                 //行号（从 1 开始）
    int column;               //列号（从 1 开始）

    // 字面量值
    int intValue = 0;     // NUMBER 类型的整数值

    Token() : type(TokenType::UNKNOWN), line(0), column(0) {}   // 默认构造函数，初始化为 UNKNOWN 类型
    Token(TokenType t, std::string lex, int ln, int col)        // 构造函数，初始化所有成员
        : type(t), lexeme(std::move(lex)), line(ln), column(col) {}
};

/// @brief Token 类型转字符串（调试用）
inline const char* tokenTypeName(TokenType t) {
    switch (t) {
        case TokenType::IF:         return "IF";
        case TokenType::ELSE:       return "ELSE";
        case TokenType::WHILE:      return "WHILE";
        case TokenType::RETURN:     return "RETURN";
        case TokenType::BREAK:      return "BREAK";
        case TokenType::CONTINUE:   return "CONTINUE";
        case TokenType::CONST:      return "CONST";
        case TokenType::INT:        return "INT";
        case TokenType::VOID:       return "VOID";
        case TokenType::NUMBER:     return "NUMBER";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::PLUS:       return "PLUS";
        case TokenType::MINUS:      return "MINUS";
        case TokenType::STAR:       return "STAR";
        case TokenType::SLASH:      return "SLASH";
        case TokenType::ASSIGN:     return "ASSIGN";
        case TokenType::EQ:         return "EQ";
        case TokenType::NEQ:        return "NEQ";
        case TokenType::LT:         return "LT";
        case TokenType::LE:         return "LE";
        case TokenType::GT:         return "GT";
        case TokenType::GE:         return "GE";
        case TokenType::MOD:        return "MOD";
        case TokenType::NOT:        return "NOT";
        case TokenType::AND:        return "AND";
        case TokenType::OR:         return "OR";
        case TokenType::LPAREN:     return "LPAREN";
        case TokenType::RPAREN:     return "RPAREN";
        case TokenType::LBRACE:     return "LBRACE";
        case TokenType::RBRACE:     return "RBRACE";
        case TokenType::SEMICOLON:  return "SEMICOLON";
        case TokenType::COMMA:      return "COMMA";
        case TokenType::EOF_TOKEN:  return "EOF";
        default:                    return "UNKNOWN";
    }
}

} // namespace MyCompiler
