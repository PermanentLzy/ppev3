#pragma once

#include "../lexer/Token.h"
#include "../lexer/Lexer.h"
#include "../ast/AST.h"
#include "ParseError.h"
#include <memory>
#include <vector>

namespace MyCompiler {

/// @brief 递归下降语法分析器
class Parser {
public:
    explicit Parser(Lexer& lexer);

    /// 解析整个程序，返回 AST 根节点（Program）
    std::unique_ptr<Program> parse();

private:
    Lexer& lexer_;
    Token  currentToken_;
    Token  previousToken_;              ///< 上一个已消耗的 Token

    // ---- 核心方法 ----
    void advance();                     // 消耗当前 Token，读下一个
    bool check(TokenType t) const;      // 检查当前 Token 类型
    bool match(TokenType t);            // 匹配并消耗
    Token consume(TokenType t, const std::string& errMsg);  // 必须匹配，否则报错

    // ---- 同步（恐慌模式错误恢复）----
    void synchronize();

    // ---- 文法规则对应函数 ----
    // CompUnit -> (Decl | FuncDef)+
    std::unique_ptr<Program> parseProgram();

    // Decl -> ConstDecl | VarDecl
    //   (全局声明，也在 parseStatement 中用于局部声明)
    std::unique_ptr<Stmt> parseDecl();

    // ConstDecl -> "const" "int" ID "=" Expr ";"
    std::unique_ptr<Stmt> parseConstDecl();

    // VarDecl -> "int" ID "=" Expr ";"
    std::unique_ptr<Stmt> parseVarDecl();

    // FuncDef -> ("int" | "void") ID "(" (Param ("," Param)*)? ")" Block
    std::unique_ptr<FunctionDecl> parseFuncDef();

    // Param -> "int" ID
    std::pair<std::string, DataType> parseParam();

    // statement -> block | ";" | exprStmt | assignment | decl
    //            | if | while | break | continue | return
    std::unique_ptr<Stmt> parseStatement();

    // assignment -> ID "=" Expr ";"
    std::unique_ptr<Stmt> parseAssignment();

    // ifStmt -> 'if' '(' expression ')' statement ('else' statement)?
    std::unique_ptr<Stmt> parseIfStmt();

    // whileStmt -> 'while' '(' expression ')' statement
    std::unique_ptr<Stmt> parseWhileStmt();

    // block -> '{' statement* '}'
    std::unique_ptr<BlockStmt> parseBlock();

    // breakStmt -> 'break' ';'
    std::unique_ptr<Stmt> parseBreakStmt();

    // continueStmt -> 'continue' ';'
    std::unique_ptr<Stmt> parseContinueStmt();

    // returnStmt -> 'return' expression? ';'
    std::unique_ptr<Stmt> parseReturnStmt();

    // 类型: 'int' | 'void'
    DataType parseType();

    // ---- 表达式层级（体现优先级）----
    // Expr -> AssignExpr
    std::unique_ptr<Expr> parseExpression();

    // AssignExpr -> LOrExpr ('=' AssignExpr)?
    //   右结合：支持链式赋值 a = b = c
    //   左值必须是标识符
    std::unique_ptr<Expr> parseAssignmentExpr();

    // LOrExpr -> LAndExpr ( '||' LAndExpr )*
    std::unique_ptr<Expr> parseLogicalOr();

    // LAndExpr -> RelExpr ( '&&' RelExpr )*
    std::unique_ptr<Expr> parseLogicalAnd();

    // RelExpr -> AddExpr ( ('<' | '>' | '<=' | '>=' | '==' | '!=') AddExpr )*
    std::unique_ptr<Expr> parseRelational();

    // AddExpr -> MulExpr ( ('+' | '-') MulExpr )*
    std::unique_ptr<Expr> parseAddition();

    // MulExpr -> UnaryExpr ( ('*' | '/' | '%') UnaryExpr )*
    std::unique_ptr<Expr> parseMultiply();

    // UnaryExpr -> ('+' | '-' | '!') UnaryExpr | PrimaryExpr
    std::unique_ptr<Expr> parseUnary();

    // PrimaryExpr -> ID | NUMBER | '(' Expr ')' | ID '(' (Expr (',' Expr)*)? ')'
    std::unique_ptr<Expr> parsePrimary();
};

} // namespace MyCompiler
