#include "AST.h"
#include "../lexer/Token.h"
#include <iostream>

namespace MyCompiler {

/// @brief AST 树形打印器（调试用，访问者模式实现）
struct ASTPrinter : ASTVisitor {
    int depth = 0;

    void indent() {     // 根据 depth 打印缩进，每层两个空格，用于表示树形结构，depth 在访问子节点时增加，访问完后减少
        for (int i = 0; i < depth; ++i)
            std::cout << "  ";
    }

    // ---- 表达式 ----
    void visit(NumberLiteral& n)   override {
        indent();
        std::cout << "NumberLiteral(" << n.value << ")\n";
    }
    void visit(IdentifierExpr& n)  override {
        // TODO: 打印缩进 + "IdentifierExpr(name)"
        indent();
        std::cout << "IdentifierExpr(" << n.name << ")\n";
    }
    void visit(BinaryExpr& n) override {
        // TODO: 打印缩进 + "BinaryExpr(op)"
        //       depth++; left->accept; right->accept; depth--;
        indent();
        std::cout << "BinaryExpr(" << tokenTypeName(n.op) << ")\n";    //op是TokenType类型，tokenTypeName是一个函数，可以将TokenType转换为字符串
        depth++;
        n.left->accept(*this);      //this是当前对象的指针，*this是当前对象的引用，访问者模式中需要传递当前访问者对象的引用
                                            //当前this是ASTPrinter对象的指针，*this是ASTPrinter对象的引用，传递给子节点访问者方法
        n.right->accept(*this);
        depth--;
    }
    void visit(UnaryExpr& n) override {
        // TODO: 打印缩进 + "UnaryExpr(op)"
        //       depth++; operand->accept; depth--;
        indent();
        std::cout << "UnaryExpr(" << tokenTypeName(n.op) << ")\n";
        depth++;
        n.operand->accept(*this);
        depth--;
    }
    void visit(AssignExpr& n) override {
        indent();
        std::cout << "AssignExpr(" << n.name << ")\n";
        depth++;
        n.value->accept(*this);
        depth--;
    }
    void visit(CallExpr& n) override {
        indent();
        std::cout << "CallExpr(" << n.funcName << ")\n";
        depth++;
        for (auto& a : n.args) a->accept(*this);
        depth--;
    }

    // ---- 语句 ----
    void visit(ExprStmt& n) override {
        indent();
        std::cout << "ExprStmt\n";
        if (n.expr) {
            depth++;
            n.expr->accept(*this);
            depth--;
        } else {
            indent();
            std::cout << "  (empty)\n";
        }
    }
    void visit(VarDeclStmt& n) override {
        indent();
        std::cout << (n.isConst ? "ConstDecl" : "VarDecl")
                  << "(" << dataTypeName(n.type) << " " << n.name << ")\n";
        if (n.init) {
            depth++;
            n.init->accept(*this);
            depth--;
        }
    }
    void visit(IfStmt& n) override {
        // TODO: 打印缩进 + "IfStmt"
        //       depth++; 打印 "Condition:"; condition->accept
        //                打印 "Then:"; thenBranch->accept
        //       if (else) 打印 "Else:"; elseBranch->accept
        //       depth--;
        indent();
        std::cout << "IfStmt\n";
        depth++;
        std::cout << "Condition:\n";
        n.condition->accept(*this);
        std::cout << "Then:\n";
        n.thenBranch->accept(*this);
        if (n.elseBranch) {     //有时候只有then分支没有else分支，所以需要判断是否存在elseBranch
            std::cout << "Else:\n";
            n.elseBranch->accept(*this);
        }
        depth--;
    }
    void visit(WhileStmt& n) override {
        // TODO: 打印缩进 + "WhileStmt"
        //       depth++; 打印 "Condition:"; condition->accept
        //                打印 "Body:"; body->accept
        //       depth--;
        indent();
        std::cout << "WhileStmt\n";
        depth++;
        std::cout << "Condition:\n";
        n.condition->accept(*this);
        std::cout << "Body:\n";
        n.body->accept(*this);
        depth--;
    }
    void visit(BreakStmt& n) override {
        // TODO: 打印缩进 + "BreakStmt"
        indent();
        std::cout << "BreakStmt\n";
    }
    void visit(ContinueStmt& n) override {
        // TODO: 打印缩进 + "ContinueStmt"
        indent();
        std::cout << "ContinueStmt\n";
    }
    void visit(ReturnStmt& n) override {
        // TODO: 打印缩进 + "ReturnStmt"；若有值则 depth++ → accept → depth--
        indent();
        std::cout << "ReturnStmt\n";
        if (n.value) {
            depth++;
            n.value->accept(*this);
            depth--;
        }
    }
    void visit(BlockStmt& n) override {
        // TODO: 打印缩进 + "Block"
        //       depth++; for (auto& s : statements) s->accept; depth--;
        indent();
        std::cout << "Block\n";
        depth++;
        for (auto& s : n.statements){   //循环访问Block中的每个语句，增加缩进表示它们是Block的子节点
                                                //逐个遍历vector中的每个元素，访问它们
            s->accept(*this);
        }
        depth--;
    }
    void visit(FunctionDecl& n) override {
        indent();
        std::cout << "FunctionDecl(" << dataTypeName(n.returnType)
                  << " " << n.name << ")\n";
        depth++;
        for (auto& p : n.params) {
            indent();
            std::cout << "Param(" << dataTypeName(p.second) << " " << p.first << ")\n";
        }
        n.body->accept(*this);
        depth--;
    }
    void visit(Program& n) override {
        std::cout << "Program\n";
        depth++;
        for (auto& s : n.statements) s->accept(*this);
        for (auto& f : n.functions) f->accept(*this);
        depth--;
    }
};

void printAST(Program& prog) {
    ASTPrinter printer;
    prog.accept(printer);
}

} // namespace MyCompiler