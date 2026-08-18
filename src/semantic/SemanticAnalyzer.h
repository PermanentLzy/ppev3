#pragma once

#include "../ast/AST.h"
#include "SymbolTable.h"
#include <string>

namespace MyCompiler {

/// @brief 语义分析器：类型检查 + 作用域分析 + 函数检查
class SemanticAnalyzer : public ASTVisitor {
public:
    SemanticAnalyzer();

    /// 对 AST 执行语义分析
    /// @returns 错误数量（0 表示无错）
    int analyze(Program& prog);

    /// 获取符号表引用（供后续阶段使用）
    SymbolTable& symbolTable() { return symtab_; }

    // ---- Visitor 实现 ----
    void visit(NumberLiteral& n)   override;
    void visit(IdentifierExpr& n)  override;
    void visit(BinaryExpr& n)      override;
    void visit(UnaryExpr& n)       override;
    void visit(AssignExpr& n)      override;
    void visit(CallExpr& n)        override;
    void visit(ExprStmt& n)        override;
    void visit(VarDeclStmt& n)     override;
    void visit(IfStmt& n)          override;
    void visit(WhileStmt& n)       override;
    void visit(BreakStmt& n)       override;
    void visit(ContinueStmt& n)    override;
    void visit(ReturnStmt& n)      override;
    void visit(BlockStmt& n)       override;
    void visit(FunctionDecl& n)    override;
    void visit(Program& n)         override;

private:
    SymbolTable symtab_;
    int         errorCount_;

    /// 当前节点推导出的类型
    DataType currentType_;

    /// 循环嵌套深度（用于检查 break/continue 是否在循环内）
    int loopDepth_ = 0;

    /// 当前函数的返回类型（用于 return 类型检查）
    DataType currentFuncReturnType_ = DataType::VOID;

    /// 当前函数是否有返回值（用于 return 类型检查）
    bool currentFuncHasReturn_ = false;

    void error(const std::string& msg);
    DataType checkBinaryOp(DataType left, TokenType op, DataType right);
    bool isArithmeticOp(TokenType op) const;
    bool isComparisonOp(TokenType op) const;
    bool isLogicalOp(TokenType op) const;   // && ||

    /// 检查语句块是否保证有 return（用于 int 函数检查）
    bool blockAlwaysReturns(const BlockStmt& block);
    /// 检查语句是否保证有 return
    bool stmtAlwaysReturns(const Stmt& stmt);
};

} // namespace MyCompiler
