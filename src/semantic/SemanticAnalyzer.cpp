#include "SemanticAnalyzer.h"
#include "../lexer/Token.h"
#include "../utils/ErrorHandler.h"
#include <iostream>

namespace MyCompiler {

SemanticAnalyzer::SemanticAnalyzer()
    : errorCount_(0), currentType_(DataType::VOID),
      loopDepth_(0), currentFuncReturnType_(DataType::VOID), currentFuncHasReturn_(false) {}

int SemanticAnalyzer::analyze(Program& prog) {
    errorCount_ = 0;
    prog.accept(*this);
    return errorCount_;
}

// ---- 表达式 ----

void SemanticAnalyzer::visit(NumberLiteral& n) {
    currentType_ = DataType::INT;
}

void SemanticAnalyzer::visit(IdentifierExpr& n) {
    auto* sym = symtab_.lookup(n.name);
    if (!sym) {
        error(MSG("未定义变量: ", "Undefined variable: ") + n.name);
        currentType_ = DataType::ERROR;
    }
    else if (!sym->initialized && !sym->isFunction) {
        error(MSG("变量未初始化: ", "Used before init: ") + n.name);
        currentType_ = DataType::ERROR;
    }
    else {
        currentType_ = sym->type;
    }
}

void SemanticAnalyzer::visit(BinaryExpr& n) {
    DataType leftType, rightType;
    n.left->accept(*this);
    leftType = currentType_;
    n.right->accept(*this);
    rightType = currentType_;
    currentType_ = checkBinaryOp(leftType, n.op, rightType);
}

void SemanticAnalyzer::visit(UnaryExpr& n) {
    n.operand->accept(*this);
    if (n.op == TokenType::MINUS || n.op == TokenType::PLUS) {
        // 一元 +/-: 操作数必须是 int, 返回 int
        if (currentType_ != DataType::INT) {
            error(MSG("一元 '+/-' 的操作数必须是 int 类型", "Unary '+/-' requires int operand"));
            currentType_ = DataType::ERROR;
        }
        // 结果保持 int
    }
    else if (n.op == TokenType::NOT) {
        // 一元 !: 操作数必须是 int (ToyC 非零为真), 返回 int (0 或 1)
        if (currentType_ != DataType::INT) {
            error(MSG("一元 '!' 的操作数必须是 int 类型", "Unary '!' requires int operand"));
            currentType_ = DataType::ERROR;
        }
        // ! 返回 int (C 语义: !0=1, !非零=0)
    }
}

void SemanticAnalyzer::visit(AssignExpr& n) {
    auto* sym = symtab_.lookup(n.name);
    if (!sym) {
        error(MSG("未定义变量: ", "Undefined variable: ") + n.name);
        currentType_ = DataType::ERROR;
        return;
    }
    // ⚠️ ToyC 约束: const 不能作为赋值左值
    if (sym->isConst) {
        error(MSG("不能修改常量 '", "Cannot assign to const '") + n.name + "'");
        currentType_ = DataType::ERROR;
        return;
    }
    n.value->accept(*this);
    DataType rhsType = currentType_;
    if (rhsType != sym->type) {
        error(MSG("赋值类型不匹配", "Type mismatch in assignment"));
        currentType_ = DataType::ERROR;
    }
    else {
        sym->initialized = true;
    }
}

void SemanticAnalyzer::visit(CallExpr& n) {
    // TODO: 查找函数符号，检查参数数量和类型
    //       设置 currentType_ 为函数返回类型
    auto* sym = symtab_.lookup(n.funcName);
    if (!sym || !sym->isFunction) {
        error(MSG("未定义的函数: ", "Undefined function: ") + n.funcName);
        currentType_ = DataType::ERROR;
        return;
    }
    // 检查参数数量
    if (n.args.size() != sym->paramTypes.size()) {
        error(MSG("函数 '", "Function '") + n.funcName +
              MSG("' 参数数量不匹配", "' argument count mismatch"));
        currentType_ = DataType::ERROR;
        return;
    }
    // 检查每个参数类型
    for (size_t i = 0; i < n.args.size(); ++i) {
        n.args[i]->accept(*this);
        if (currentType_ != sym->paramTypes[i]) {
            error(MSG("函数 '", "Function '") + n.funcName +
                  MSG("' 参数类型不匹配", "' argument type mismatch"));
        }
    }
    currentType_ = sym->returnType;
}

// ---- 语句 ----

void SemanticAnalyzer::visit(ExprStmt& n) {
    if (n.expr) {
        n.expr->accept(*this);
    }
}

void SemanticAnalyzer::visit(VarDeclStmt& n) {
    // 检查当前作用域重复声明
    Symbol sym;
    sym.name = n.name;
    sym.type = n.type;
    sym.scopeLevel = symtab_.currentLevel();
    sym.initialized = (n.init != nullptr);
    sym.isConst = n.isConst;
    // 全局非 const 变量按 C 语义默认初始化为 0，允许后续读取
    if (symtab_.atGlobalScope() && !n.isConst) {
        sym.initialized = true;
    }
    if (!symtab_.define(sym)) {
        error(MSG("变量 '", "Variable '") + n.name +
              MSG("' 在当前作用域中已定义", "' already defined in this scope."));
        return;
    }
    if (n.init) {
        n.init->accept(*this);
        if (currentType_ != n.type) {
            error(MSG("初始化类型不匹配", "Type mismatch in initialization"));
            currentType_ = DataType::ERROR;
        }
    }
}

void SemanticAnalyzer::visit(IfStmt& n) {
    // ToyC: 条件为 int 类型 (非零为真)
    n.condition->accept(*this);
    if (currentType_ != DataType::INT && currentType_ != DataType::ERROR) {
        error(MSG("'if' 条件必须是 int 类型", "Condition in 'if' must be int type."));
    }
    n.thenBranch->accept(*this);
    if (n.elseBranch) {
        n.elseBranch->accept(*this);
    }
}

void SemanticAnalyzer::visit(WhileStmt& n) {
    n.condition->accept(*this);
    if (currentType_ != DataType::INT && currentType_ != DataType::ERROR) {
        error(MSG("'while' 条件必须是 int 类型", "Condition in 'while' must be int type."));
    }
    loopDepth_++;
    n.body->accept(*this);
    loopDepth_--;
}

void SemanticAnalyzer::visit(BreakStmt& n) {
    // ⚠️ break 只能在循环中
    if (loopDepth_ <= 0) {
        error(MSG("'break' 只能在循环中使用", "'break' can only be used inside a loop."));
    }
}

void SemanticAnalyzer::visit(ContinueStmt& n) {
    // ⚠️ continue 只能在循环中
    if (loopDepth_ <= 0) {
        error(MSG("'continue' 只能在循环中使用", "'continue' can only be used inside a loop."));
    }
}

void SemanticAnalyzer::visit(ReturnStmt& n) {
    currentFuncHasReturn_ = true;
    if (n.value) {
        n.value->accept(*this);
        if (currentType_ != currentFuncReturnType_ && currentFuncReturnType_ != DataType::ERROR) {
            error(MSG("return 类型与函数返回类型不匹配", "Return type does not match function return type."));
        }
    } else {
        // return; (无返回值)
        if (currentFuncReturnType_ != DataType::VOID) {
            error(MSG("int 函数必须有返回值", "Non-void function must return a value."));
        }
    }
}

void SemanticAnalyzer::visit(BlockStmt& n) {
    symtab_.enterScope();
    for (auto& s : n.statements) {
        s->accept(*this);
    }
    symtab_.exitScope();
}

void SemanticAnalyzer::visit(FunctionDecl& n) {
    // 注意: 函数签名已在 visit(Program) 的第一遍中注册到全局作用域
    // 这里只做: 设置函数上下文 → 进入作用域 → 注册形参 → 分析函数体

    // 1. 进入函数体上下文
    DataType savedReturnType = currentFuncReturnType_;
    bool savedHasReturn = currentFuncHasReturn_;
    currentFuncReturnType_ = n.returnType;
    currentFuncHasReturn_ = false;

    // 2. 进入函数作用域 + 注册形参
    symtab_.enterScope();
    for (auto& p : n.params) {
        Symbol paramSym;
        paramSym.name = p.first;
        paramSym.type = p.second;
        paramSym.scopeLevel = symtab_.currentLevel();
        paramSym.initialized = true;
        symtab_.define(paramSym);
    }
    // 3. 分析函数体
    n.body->accept(*this);
    symtab_.exitScope();

    // 4. 恢复上下文
    // 检查 int 函数所有路径是否都有 return
    if (n.returnType == DataType::INT)
    {
        if (!blockAlwaysReturns(*n.body))
        {
            error(MSG("int 函数 '", "Non-void function '") + n.name +
                  MSG("' 并非所有路径都有返回值", "' does not return on all paths."));
        }
    }
    currentFuncReturnType_ = savedReturnType;
    currentFuncHasReturn_ = savedHasReturn;
}

void SemanticAnalyzer::visit(Program& n) {
    // 先注册所有函数签名（支持前向引用）
    for (auto& f : n.functions) {
        auto* existing = symtab_.lookupCurrent(f->name);
        if (existing) {
            error(MSG("函数 '", "Function '") + f->name +
                  MSG("' 已定义", "' already defined."));
            continue;
        }
        Symbol funcSym;
        funcSym.name = f->name;
        funcSym.type = f->returnType;
        funcSym.scopeLevel = 0;
        funcSym.initialized = true;
        funcSym.isFunction = true;
        funcSym.returnType = f->returnType;
        for (auto& p : f->params) {
            funcSym.paramTypes.push_back(p.second);
        }
        symtab_.define(funcSym);
    }

    // 再分析全局变量声明
    for (auto& s : n.statements) {
        s->accept(*this);
    }

    // 最后分析函数体
    for (auto& f : n.functions) {
        f->accept(*this);
    }

    // 检查是否存在 main 函数 (返回 int, 无参数)
    auto* mainSym = symtab_.lookup("main");
    if (!mainSym || !mainSym->isFunction) {
        error(MSG("程序必须包含 'main' 函数", "Program must contain a 'main' function."));
    } else if (mainSym->returnType != DataType::INT) {
        error(MSG("'main' 函数必须返回 int 类型", "'main' must return int."));
    } else if (!mainSym->paramTypes.empty()) {
        error(MSG("'main' 函数不能有参数", "'main' must have no parameters."));
    }
}

// ================================================================
//  辅助函数
// ================================================================

void SemanticAnalyzer::error(const std::string& msg) {
    std::cout << "[Semantic Error] " << msg << '\n';
    errorCount_++;
}

bool SemanticAnalyzer::isArithmeticOp(TokenType op) const {
    return op == TokenType::PLUS || op == TokenType::MINUS ||
           op == TokenType::STAR || op == TokenType::SLASH ||
           op == TokenType::MOD;
}

bool SemanticAnalyzer::isComparisonOp(TokenType op) const {
    return op == TokenType::EQ  || op == TokenType::NEQ ||
           op == TokenType::LT  || op == TokenType::LE  ||
           op == TokenType::GT  || op == TokenType::GE;
}

bool SemanticAnalyzer::isLogicalOp(TokenType op) const {
    return op == TokenType::AND || op == TokenType::OR;
}

DataType SemanticAnalyzer::checkBinaryOp(DataType left, TokenType op, DataType right) {
    if (left == DataType::ERROR || right == DataType::ERROR)
        return DataType::ERROR;

    if (isArithmeticOp(op)) {
        if (left != DataType::INT || right != DataType::INT) {
            error(MSG("算术运算符要求操作数为 int 类型", "Arithmetic operators require int operands."));
            return DataType::ERROR;
        }
        return DataType::INT;
    }

    if (isComparisonOp(op)) {
        if (left != right) {
            error(MSG("比较运算符要求操作数类型相同", "Comparison operands must have same type."));
            return DataType::ERROR;
        }
        // ToyC: 比较运算返回 int (0 或 1)
        return DataType::INT;
    }

    if (isLogicalOp(op)) {
        // ToyC: && 和 || 操作数必须是 int (非零为真), 返回 int
        if (left != DataType::INT || right != DataType::INT) {
            error(MSG("逻辑运算符要求操作数为 int 类型", "Logical operators require int operands."));
            return DataType::ERROR;
        }
        return DataType::INT;
    }

    error(MSG("不支持的二元运算符", "Unsupported binary operator."));
    return DataType::ERROR;
}

// ================================================================
//  控制流分析：检查语句/语句块是否保证有 return
// ================================================================

bool SemanticAnalyzer::stmtAlwaysReturns(const Stmt& stmt) {
    // ReturnStmt 保证有 return
    if (dynamic_cast<const ReturnStmt*>(&stmt))
        return true;

    // BlockStmt: 只要有任意一条语句保证 return，则整个块返回
    if (auto* block = dynamic_cast<const BlockStmt*>(&stmt)) {
        return blockAlwaysReturns(*block);
    }

    // IfStmt: 两个分支都保证 return 才返回 true
    if (auto* ifStmt = dynamic_cast<const IfStmt*>(&stmt)) {
        if (!ifStmt->elseBranch)
            return false; // 无 else 分支，不保证 return
        return stmtAlwaysReturns(*ifStmt->thenBranch) &&
               stmtAlwaysReturns(*ifStmt->elseBranch);
    }

    // WhileStmt: 不确定是否执行，不保证 return
    // ExprStmt, VarDeclStmt, BreakStmt, ContinueStmt 等不保证 return
    return false;
}

bool SemanticAnalyzer::blockAlwaysReturns(const BlockStmt& block) {
    // 从后往前找：如果最后一条语句保证 return，则整个块保证 return
    for (auto it = block.statements.rbegin(); it != block.statements.rend(); ++it) {
        if (stmtAlwaysReturns(**it))
            return true;
        // 如果是 if-else 且两个分支都 return，也算
        // （已在 stmtAlwaysReturns 中处理）
    }
    return false;
}

} // namespace MyCompiler
