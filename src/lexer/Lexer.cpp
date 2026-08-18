#include "Lexer.h"
#include "Token.h"

namespace MyCompiler {

Lexer::Lexer(const std::string& source)
    : source_(source), current_(0), line_(1), column_(1) {
    //初始化
}

char Lexer::advance() {
    // TODO: 读取一个字符，推进指针，更新行列号
    if (isAtEnd())           // 已到达末尾，返回特殊值
        return '\0';
    char c = source_[current_++];         //字符串存在source_中，指针为current_，先取后加
    if (c == '\n'){
        line_++;
        column_ = 1;
    }
    else {
        column_++;
    }
    return c;
}

char Lexer::peekChar() const {
    // TODO: 向前看一个字符（不推进）
    if (isAtEnd()) 
        return '\0';
    char c = source_[current_];
    return c;
}

char Lexer::peekNext() const {
    // TODO: 向前看两个字符
    if (isAtEnd() || current_ + 1 >= source_.size())  //确保后一个字符落在合法范围内 
        return '\0';
    char c = source_[current_ + 1];
    return c;
}

void Lexer::skipWhitespace() {
    // 跳过空白字符和注释（单行 // 和多行 /* */）
    while (!isAtEnd()){
        char c = peekChar();
        if (isspace(c)){  //空白字符
            advance();
        }
        else if (c == '/') {
            if (peekNext() == '/') {       // 单行注释 //
                skipLineComment();
            } else if (peekNext() == '*') { // 多行注释 /* */
                skipBlockComment();
            } else {
                break;  // 除法运算符，留给 scanOperator 处理
            }
        }
        else {
            break;
        }
    }
}

void Lexer::skipLineComment() {
    // 跳过 // 注释直到行尾
    while (!isAtEnd() && peekChar() != '\n'){
        advance();
    }
}

void Lexer::skipBlockComment() {
    // 跳过 /* */ 多行注释（支持嵌套？ToyC 不需要嵌套）
    advance(); // 跳过 /
    advance(); // 跳过 *
    while (!isAtEnd()) {
        if (peekChar() == '*' && peekNext() == '/') {
            advance(); // 跳过 *
            advance(); // 跳过 /
            return;
        }
        advance();
    }
    // TODO: 如果到达文件末尾仍未闭合，报错（未闭合的多行注释）
}

Token Lexer::scanNumber() {
    // TODO: 支持十进制、十六进制（0x 前缀）和二进制（0b 前缀）整数
    int startLine = line_;
    int startCol  = column_;

    // 第一步：收集所有可能属于数字的字符
    std::string numStr;
    while (!isAtEnd() && (isdigit(peekChar()) || 
           (numStr.size() >= 1 && tolower(peekChar()) == 'x') ||      // 0x 前缀
           (numStr.size() >= 1 && tolower(peekChar()) == 'b') ||      // 0b 前缀
           (numStr.size() >= 2 && isxdigit(peekChar())))) {           // 十六进制数字
        numStr += advance();
    }

    // 第二步：根据前缀解析值
    int value = 0;
    if (numStr.size() >= 2 && numStr[0] == '0') {
        if (tolower(numStr[1]) == 'x') {    // 0x 前缀 → 十六进制
            value = std::stoi(numStr.substr(2), nullptr, 16);
        } else if (tolower(numStr[1]) == 'b') {    // 0b 前缀 → 二进制
            value = std::stoi(numStr.substr(2), nullptr, 2);
        } else {
            value = std::stoi(numStr, nullptr, 8);  // 0 开头 → 八进制
        }
    } else {
        value = std::stoi(numStr, nullptr, 10);      // 十进制
    }

    Token tok(TokenType::NUMBER, numStr, startLine, startCol);
    tok.intValue = value;
    return tok;
}

Token Lexer::scanIdentifierOrKeyword() {
    // TODO: 扫描标识符，查关键字表
    int startLine = line_;
    int startCol  = column_;
    std::string identStr;
    while (!isAtEnd() && (isalnum(peekChar()) || peekChar() == '_')){    //标识符由字母、数字、下划线组成
        identStr += advance();
    }
    const auto& keywords = keywordMap();     //Token.h中定义特殊字符————关键字
    auto it = keywords.find(identStr);
    if (it != keywords.end()){
        // 是关键字
        return Token(it->second, identStr, startLine, startCol);    // 关键字的 TokenType 由 keywordMap 定义
    }
    return Token(TokenType::IDENTIFIER, identStr, startLine, startCol);
}

Token Lexer::scanOperator() {
    // TODO: 扫描运算符（含双字符运算符 peek）
    int startLine = line_;
    int startCol  = column_;
    char c = advance();
    switch (c) {
        case '+': return Token(TokenType::PLUS, "+", startLine, startCol);
        case '-': return Token(TokenType::MINUS, "-", startLine, startCol);
        case '*': return Token(TokenType::STAR, "*", startLine, startCol);
        case '/': return Token(TokenType::SLASH, "/", startLine, startCol);
        case '%': return Token(TokenType::MOD, "%", startLine, startCol);
        case '(': return Token(TokenType::LPAREN, "(", startLine, startCol);
        case ')': return Token(TokenType::RPAREN, ")", startLine, startCol);
        case '{': return Token(TokenType::LBRACE, "{", startLine, startCol);
        case '}': return Token(TokenType::RBRACE, "}", startLine, startCol);
        case ';': return Token(TokenType::SEMICOLON, ";", startLine, startCol);
        case ',': return Token(TokenType::COMMA, ",", startLine, startCol);
        case '=':
            if (peekChar() == '=') {
                advance();
                return Token(TokenType::EQ, "==", startLine, startCol);
            }
            return Token(TokenType::ASSIGN, "=", startLine, startCol);
        case '!':
            if (peekChar() == '=') {
                advance();
                return Token(TokenType::NEQ, "!=", startLine, startCol);
            }
            return Token(TokenType::NOT, "!", startLine, startCol);
        case '<':
            if (peekChar() == '=') {
                advance();
                return Token(TokenType::LE, "<=", startLine, startCol);
            }
            return Token(TokenType::LT, "<", startLine, startCol);
        case '>':
            if (peekChar() == '=') {
                advance();
                return Token(TokenType::GE, ">=", startLine, startCol);
            }
            return Token(TokenType::GT, ">", startLine, startCol);
        case '&':
            if (peekChar() == '&') {
                advance();
                return Token(TokenType::AND, "&&", startLine, startCol);
            }
            // ToyC 不支持按位与 &，但单 & 报错太严格，返回 UNKNOWN
            return Token(TokenType::UNKNOWN, "&", startLine, startCol);
        case '|':
            if (peekChar() == '|') {
                advance();
                return Token(TokenType::OR, "||", startLine, startCol);
            }
            return Token(TokenType::UNKNOWN, "|", startLine, startCol);
        case '^':
            return Token(TokenType::UNKNOWN, "^", startLine, startCol);
        default:
            break;  // 未知字符
    }
    return Token(TokenType::UNKNOWN, std::string(1, c), startLine, startCol);
}

Token Lexer::nextToken() {
    // TODO: 跳过空白，根据首字符分发到 scanNumber / scanIdentifierOrKeyword / scanOperator
    skipWhitespace();
    if (isAtEnd()) {
        return Token(TokenType::EOF_TOKEN, "", line_, column_);
    }
    char c = peekChar();
    if (isdigit(c)){// 数字
        return scanNumber();
    }
    else if (isalpha(c) || c == '_'){// 标识符或关键字
        return scanIdentifierOrKeyword();
    }
    else{
        return scanOperator();// 其他情况当作运算符处理
    }
}

Token Lexer::peek() {
    // TODO: 保存状态 → nextToken() → 恢复状态
    int savedCurrent = current_;
    int savedLine = line_;
    int savedColumn = column_;
    Token tok = nextToken();
    current_ = savedCurrent;
    line_ = savedLine;
    column_ = savedColumn;
    return tok;
}

std::vector<Token> Lexer::scanAll() {
    // TODO: 循环调用 nextToken() 直到 EOF
    std::vector<Token> tokens;
    while (true) {
        Token tok = nextToken();
        tokens.push_back(tok);
        if (tok.type == TokenType::EOF_TOKEN) {
            break;
        }
    }
    return tokens;
}

} // namespace MyCompiler

