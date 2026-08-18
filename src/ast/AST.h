#pragma once

#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

namespace MyCompiler {

// ---- 前向声明 ----
struct ASTVisitor;      // 访问者接口

// ---- 数据类型枚举 ----
/// @note ToyC 只有 int 和 void 两种类型
enum class DataType { INT, VOID, ERROR };

inline const char* dataTypeName(DataType t) {
    switch (t) {
        case DataType::INT:  return "int";
        case DataType::VOID: return "void";
        default:             return "ERROR";
    }
}

// ---- TokenType 前向声明（避免循环依赖）----
enum class TokenType;

// ================================================================
//  AST 节点基类
// ================================================================

struct ASTNode {
    virtual ~ASTNode() = default;       // 虚析构函数，确保派生类对象被正确销毁
    virtual void accept(ASTVisitor& v) = 0;     // 接受访问者，派生类必须实现
};

// ================================================================
//  表达式节点
// ================================================================

struct Expr : ASTNode {};

/// 整数字面量
struct NumberLiteral : Expr {
    int value;
    explicit NumberLiteral(int v) : value(v) {}
    void accept(ASTVisitor& v) override;
};

/// 标识符引用
struct IdentifierExpr : Expr {
    std::string name;
    explicit IdentifierExpr(std::string n) : name(std::move(n)) {}
    void accept(ASTVisitor& v) override;
};

/// 二元运算：left op right
struct BinaryExpr : Expr {
    std::unique_ptr<Expr> left;
    TokenType op;
    std::unique_ptr<Expr> right;
    BinaryExpr(std::unique_ptr<Expr> l, TokenType o, std::unique_ptr<Expr> r)
        : left(std::move(l)), op(o), right(std::move(r)) {}
    void accept(ASTVisitor& v) override;
};

/// 一元运算
struct UnaryExpr : Expr {
    TokenType op;
    std::unique_ptr<Expr> operand;
    UnaryExpr(TokenType o, std::unique_ptr<Expr> e)
        : op(o), operand(std::move(e)) {}
    void accept(ASTVisitor& v) override;
};

/// 赋值表达式（变量 = 值）
struct AssignExpr : Expr {
    std::string name;
    std::unique_ptr<Expr> value;
    AssignExpr(std::string n, std::unique_ptr<Expr> v)
        : name(std::move(n)), value(std::move(v)) {}
    void accept(ASTVisitor& v) override;
};

/// 函数调用表达式: ID "(" (Expr ("," Expr)*)? ")"
struct CallExpr : Expr {
    std::string funcName;
    std::vector<std::unique_ptr<Expr>> args;
    CallExpr(std::string name, std::vector<std::unique_ptr<Expr>> a)
        : funcName(std::move(name)), args(std::move(a)) {}
    void accept(ASTVisitor& v) override;
};

// ================================================================
//  语句节点
// ================================================================

struct Stmt : ASTNode {};

/// 表达式语句（expression ;）
struct ExprStmt : Stmt {
    std::unique_ptr<Expr> expr;
    explicit ExprStmt(std::unique_ptr<Expr> e) : expr(std::move(e)) {}
    void accept(ASTVisitor& v) override;
};

/// 变量/常量声明: (const)? int ID = Expr ;
/// @note ToyC 要求所有声明必须初始化，init 不应为 nullptr
struct VarDeclStmt : Stmt {
    DataType type;       // 当前仅 INT
    std::string name;
    std::unique_ptr<Expr> init;  // 初始化表达式（必须存在）
    bool isConst = false;        // 是否为 const 声明
    VarDeclStmt(DataType t, std::string n, std::unique_ptr<Expr> i, bool c = false)
        : type(t), name(std::move(n)), init(std::move(i)), isConst(c) {}
    void accept(ASTVisitor& v) override;
};

/// if 语句
struct IfStmt : Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> thenBranch;
    std::unique_ptr<Stmt> elseBranch;  // 可为 nullptr
    IfStmt(std::unique_ptr<Expr> cond,
           std::unique_ptr<Stmt> thenB,
           std::unique_ptr<Stmt> elseB = nullptr)
        : condition(std::move(cond)),
          thenBranch(std::move(thenB)),
          elseBranch(std::move(elseB)) {}
    void accept(ASTVisitor& v) override;
};

/// while 语句
struct WhileStmt : Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> body;
    WhileStmt(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> b)
        : condition(std::move(cond)), body(std::move(b)) {}
    void accept(ASTVisitor& v) override;
};

/// break 语句
struct BreakStmt : Stmt {
    void accept(ASTVisitor& v) override;
};

/// continue 语句
struct ContinueStmt : Stmt {
    void accept(ASTVisitor& v) override;
};

/// return 语句（可带返回值）
struct ReturnStmt : Stmt {
    std::unique_ptr<Expr> value;  // 可为 nullptr（即 return;）
    explicit ReturnStmt(std::unique_ptr<Expr> v = nullptr)
        : value(std::move(v)) {}
    void accept(ASTVisitor& v) override;
};

/// 代码块
struct BlockStmt : Stmt {
    std::vector<std::unique_ptr<Stmt>> statements;
    void accept(ASTVisitor& v) override;
};

/// 函数定义: ("int"|"void") ID "(" (Param ("," Param)*)? ")" Block
struct FunctionDecl : ASTNode {
    DataType returnType;                        // INT 或 VOID
    std::string name;
    std::vector<std::pair<std::string, DataType>> params; // 参数名 + 类型（仅 INT）
    std::unique_ptr<BlockStmt> body;
    FunctionDecl(DataType rt, std::string n,
                 std::vector<std::pair<std::string, DataType>> p,
                 std::unique_ptr<BlockStmt> b)
        : returnType(rt), name(std::move(n)), params(std::move(p)), body(std::move(b)) {}
    void accept(ASTVisitor& v) override;
};

/// 程序根节点: 全局声明 + 函数定义
struct Program : ASTNode {
    std::vector<std::unique_ptr<Stmt>> statements;          // 全局变量/常量声明
    std::vector<std::unique_ptr<FunctionDecl>> functions;   // 函数定义
    void accept(ASTVisitor& v) override;
};

// ================================================================
//  访问者接口
// ================================================================

struct ASTVisitor {
    virtual ~ASTVisitor() = default;

    // 表达式
    virtual void visit(NumberLiteral& n)   = 0;
    virtual void visit(IdentifierExpr& n)  = 0;
    virtual void visit(BinaryExpr& n)      = 0;
    virtual void visit(UnaryExpr& n)       = 0;
    virtual void visit(AssignExpr& n)      = 0;
    virtual void visit(CallExpr& n)        = 0;

    // 语句
    virtual void visit(ExprStmt& n)        = 0;
    virtual void visit(VarDeclStmt& n)     = 0;
    virtual void visit(IfStmt& n)          = 0;
    virtual void visit(WhileStmt& n)       = 0;
    virtual void visit(BreakStmt& n)       = 0;
    virtual void visit(ContinueStmt& n)    = 0;
    virtual void visit(ReturnStmt& n)      = 0;
    virtual void visit(BlockStmt& n)       = 0;
    virtual void visit(FunctionDecl& n)    = 0;
    virtual void visit(Program& n)         = 0;
};

// ================================================================
//  accept 实现（在声明后定义）
// ================================================================

inline void NumberLiteral::accept(ASTVisitor& v)   { v.visit(*this); }
inline void IdentifierExpr::accept(ASTVisitor& v)  { v.visit(*this); }
inline void BinaryExpr::accept(ASTVisitor& v)      { v.visit(*this); }
inline void UnaryExpr::accept(ASTVisitor& v)       { v.visit(*this); }
inline void AssignExpr::accept(ASTVisitor& v)      { v.visit(*this); }
inline void CallExpr::accept(ASTVisitor& v)        { v.visit(*this); }
inline void ExprStmt::accept(ASTVisitor& v)        { v.visit(*this); }
inline void VarDeclStmt::accept(ASTVisitor& v)     { v.visit(*this); }
inline void IfStmt::accept(ASTVisitor& v)          { v.visit(*this); }
inline void WhileStmt::accept(ASTVisitor& v)       { v.visit(*this); }
inline void BreakStmt::accept(ASTVisitor& v)       { v.visit(*this); }
inline void ContinueStmt::accept(ASTVisitor& v)    { v.visit(*this); }
inline void ReturnStmt::accept(ASTVisitor& v)      { v.visit(*this); }
inline void BlockStmt::accept(ASTVisitor& v)       { v.visit(*this); }
inline void FunctionDecl::accept(ASTVisitor& v)    { v.visit(*this); }
inline void Program::accept(ASTVisitor& v)         { v.visit(*this); }

} // namespace MyCompiler