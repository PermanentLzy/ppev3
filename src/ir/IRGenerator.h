#pragma once

#include "../ast/AST.h"
#include "ThreeAddrCode.h"
#include "../lexer/Token.h"
#include <memory>
#include <string>
#include <stack>
#include <unordered_map>
#include <vector>

namespace MyCompiler {

/// @brief AST -> 三地址码转换器（后序遍历 AST）
class IRGenerator : public ASTVisitor {
public:
    IRGenerator();

    /// 从 AST 生成三地址码程序
    std::unique_ptr<TACProgram> generate(Program& ast);

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
    std::unique_ptr<TACProgram> program_;

    /// 当前表达式求值结果的临时变量名
    std::string result_;

    /// break/continue 标签栈 (用于嵌套循环)
    std::stack<std::pair<std::string, std::string>> loopLabels_;

    /// 当前函数的返回类型 (用于 void 函数检查)
    DataType currentFuncReturnType_ = DataType::VOID;

    /// ---- 作用域变量名管理 ----
    /// 作用域栈：每个作用域是一个 原始名→唯一名 的映射
    std::vector<std::unordered_map<std::string, std::string>> scopeMaps_;
    /// 变量名计数器（生成唯一后缀）
    int varCounter_ = 0;

    /// 进入新作用域
    void enterScope();
    /// 退出当前作用域
    void exitScope();
    /// 在当前作用域注册变量（返回唯一名）
    std::string registerVar(const std::string& originalName);
    /// 从内向外查找变量的唯一名；找不到返回原始名（全局变量）
    std::string resolveVar(const std::string& originalName);

    /// 辅助：生成新临时变量
    std::string newTemp() { return program_->newTemp(); }

    /// 辅助：生成新标签
    std::string newLabel(const std::string& prefix = "L") {
        return program_->newLabel(prefix);
    }

    /// 辅助：添加指令
    void emit(const TACInstruction& instr) {
        program_->emit(instr);
    }

    /// tokenType -> 运算符字符串
    std::string opToString(TokenType op) const;
};

} // namespace MyCompiler
