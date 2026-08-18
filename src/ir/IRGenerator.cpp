#include "IRGenerator.h"
#include "ThreeAddrCode.h"

namespace MyCompiler {

IRGenerator::IRGenerator() = default;

std::unique_ptr<TACProgram> IRGenerator::generate(Program& ast) {
    // TODO: 生成三地址码程序的入口方法，首先创建一个新的 TACProgram 对象，然后让 AST 根节点接受当前 IRGenerator 访问者，最后返回生成的 TACProgram
    program_ = std::make_unique<TACProgram>();
    ast.accept(*this);
    return std::move(program_);
}

std::string IRGenerator::opToString(TokenType op) const {
    // TODO: 将 TokenType 转为运算符字符串 "+", "-", "*", "/", "==", "!=", "<", ... 
    switch (op) {
        case TokenType::PLUS: return "+";
        case TokenType::MINUS: return "-";
        case TokenType::STAR: return "*";
        case TokenType::SLASH: return "/";
        case TokenType::EQ: return "==";
        case TokenType::NEQ: return "!=";
        case TokenType::LT: return "<";
        case TokenType::LE: return "<=";
        case TokenType::GT: return ">";
        case TokenType::GE: return ">=";
        case TokenType::AND: return "&&";
        case TokenType::OR:  return "||";
        case TokenType::MOD: return "%";
        case TokenType::NOT: return "!";
        default: break;
    }
    return "?";
}

// ---- 表达式（后序遍历，每处理一个节点生成对应的 TAC 指令）----

// ---- 作用域管理 ----

void IRGenerator::enterScope() {
    scopeMaps_.push_back({});
}

void IRGenerator::exitScope() {
    if (scopeMaps_.size() > 1)
        scopeMaps_.pop_back();
}

std::string IRGenerator::registerVar(const std::string& originalName) {
    std::string unique = originalName + "." + std::to_string(varCounter_++);
    scopeMaps_.back()[originalName] = unique;
    return unique;
}

std::string IRGenerator::resolveVar(const std::string& originalName) {
    for (auto it = scopeMaps_.rbegin(); it != scopeMaps_.rend(); ++it) {
        auto found = it->find(originalName);
        if (found != it->end())
            return found->second;
    }
    return originalName;
}

void IRGenerator::visit(NumberLiteral& n) {
    // TODO: 生成一个新的临时变量 t，将常量 n.value 赋值给 t，然后将 t 作为当前表达式的结果
    auto t = newTemp();
    emit(TACInstruction{TACType::ASSIGN, TACOperand::temp(t), TACOperand::constInt(n.value)});
    result_ = t;
}

void IRGenerator::visit(IdentifierExpr& n) {
    result_ = resolveVar(n.name);
}

void IRGenerator::visit(BinaryExpr& n) {
    // TODO: 短路求值 — && 和 || 需要特殊处理 (用 IF_GOTO 实现)
    //       对于 &&: 先求值 left, 若为 0 则跳过 right, 结果直接为 0
    //       对于 ||: 先求值 left, 若非 0 则跳过 right, 结果直接为 1
    //       其他运算符走下面的通用路径
    if (n.op == TokenType::AND || n.op == TokenType::OR) {
        // 短路求值：
        //   AND: left==0 → 短路得0; left!=0 → 结果取 right 的值
        //   OR:  left!=0 → 短路得1; left==0 → 结果取 right 的值
        auto rightLabel = newLabel("L_right");
        auto endLabel   = newLabel("L_end");
        auto result     = newTemp();

        n.left->accept(*this);
        std::string leftResult = result_;

        if (n.op == TokenType::AND) {
            // if left != 0 → 跳到 L_right 计算右边
            emit(TACInstruction{TACType::IF_GOTO, TACOperand::none(),
                TACOperand::var(leftResult), TACOperand::none(), "", rightLabel});
            // left == 0：短路，result = 0
            emit(TACInstruction{TACType::ASSIGN, TACOperand::temp(result),
                TACOperand::constInt(0), TACOperand::none()});
            emit(TACInstruction{TACType::GOTO, TACOperand::none(), TACOperand::none(),
                TACOperand::none(), "", endLabel});
            // L_right:
            emit(TACInstruction{TACType::LABEL, TACOperand::none(), TACOperand::none(),
                TACOperand::none(), "", rightLabel});
            n.right->accept(*this);
            // 规范化：将右值转为 0/1（!!right）
            auto norm = newTemp();
            emit(TACInstruction{TACType::UNARY, TACOperand::temp(norm),
                TACOperand::var(result_), TACOperand::none(), "!"});
            emit(TACInstruction{TACType::UNARY, TACOperand::temp(result),
                TACOperand::var(norm), TACOperand::none(), "!"});
        } else { // OR
            // if left != 0 → 短路，result = 1
            emit(TACInstruction{TACType::IF_GOTO, TACOperand::none(),
                TACOperand::var(leftResult), TACOperand::none(), "", rightLabel});
            // left == 0：需要计算 right
            n.right->accept(*this);
            // 规范化：将右值转为 0/1
            auto norm = newTemp();
            emit(TACInstruction{TACType::UNARY, TACOperand::temp(norm),
                TACOperand::var(result_), TACOperand::none(), "!"});
            emit(TACInstruction{TACType::UNARY, TACOperand::temp(result),
                TACOperand::var(norm), TACOperand::none(), "!"});
            emit(TACInstruction{TACType::GOTO, TACOperand::none(), TACOperand::none(),
                TACOperand::none(), "", endLabel});
            // L_right（OR 中作为短路标签）:
            emit(TACInstruction{TACType::LABEL, TACOperand::none(), TACOperand::none(),
                TACOperand::none(), "", rightLabel});
            emit(TACInstruction{TACType::ASSIGN, TACOperand::temp(result),
                TACOperand::constInt(1), TACOperand::none()});
        }
        // L_end:
        emit(TACInstruction{TACType::LABEL, TACOperand::none(), TACOperand::none(),
            TACOperand::none(), "", endLabel});
        result_ = result;
        return;
    }
    n.left->accept(*this);
    std::string leftResult = result_;
    n.right->accept(*this);
    std::string rightResult = result_;
    auto t = newTemp();
    emit(TACInstruction{TACType::BINARY, TACOperand::temp(t), TACOperand::var(leftResult), TACOperand::var(rightResult), opToString(n.op)});
    result_ = t;
}

void IRGenerator::visit(UnaryExpr& n) {
    // TODO: operand->accept → operandResult
    //       t = newTemp()
    //       emit(UNARY, temp(t), var(operandResult), none, opToString(n.op))
    //       result_ = t
    n.operand->accept(*this);
    auto t = newTemp();
    emit(TACInstruction{TACType::UNARY, TACOperand::temp(t), TACOperand::var(result_), TACOperand::none(), opToString(n.op)});
    result_ = t;
}

void IRGenerator::visit(AssignExpr& n) {
    std::string uniqueName = resolveVar(n.name);
    if (auto* num = dynamic_cast<NumberLiteral*>(n.value.get())) {
        emit(TACInstruction{TACType::ASSIGN, TACOperand::var(uniqueName),
            TACOperand::constInt(num->value), TACOperand::none()});
    } else {
        n.value->accept(*this);
        emit(TACInstruction{TACType::ASSIGN, TACOperand::var(uniqueName),
            TACOperand::var(result_), TACOperand::none()});
    }
    result_ = uniqueName;
}

void IRGenerator::visit(CallExpr& n) {
    // 1. 参数求值并发射 PARAM 指令（从左到右）
    for (auto& arg : n.args) {
        arg->accept(*this);
        // 发射 param 指令：将参数值标记为传递给下一个 call
        emit(TACInstruction{TACType::PARAM, TACOperand::none(),
            TACOperand::var(result_), TACOperand::none()});
    }

    // 2. 发射 CALL 指令
    //    label = 函数名, lhs.intValue = 参数个数
    auto retTemp = newTemp();
    TACInstruction callInstr;
    callInstr.type   = TACType::CALL;
    callInstr.label  = n.funcName;
    callInstr.result = TACOperand::temp(retTemp);
    callInstr.lhs    = TACOperand::constInt(static_cast<int>(n.args.size()));
    emit(callInstr);

    result_ = retTemp;
}

// ---- 语句 ----

void IRGenerator::visit(ExprStmt& n) {
    // 表达式语句：求值表达式，丢弃结果
    if (n.expr) {
        n.expr->accept(*this);
    }
}

void IRGenerator::visit(VarDeclStmt& n) {
    std::string uniqueName = registerVar(n.name);
    if (n.init) {
        // 优化：常量 init 跳过中间 temp
        if (auto* num = dynamic_cast<NumberLiteral*>(n.init.get())) {
            emit(TACInstruction{TACType::ASSIGN, TACOperand::var(uniqueName),
                TACOperand::constInt(num->value), TACOperand::none()});
        } else {
            n.init->accept(*this);
            emit(TACInstruction{TACType::ASSIGN, TACOperand::var(uniqueName),
                TACOperand::var(result_), TACOperand::none()});
        }
    } else if (scopeMaps_.size() == 1) {
        // 全局变量无初始化：发射 counter = 0（C 语义默认 0），
        // 让 CodeGen 第 1 遍能识别为全局变量
        emit(TACInstruction{TACType::ASSIGN, TACOperand::var(uniqueName),
            TACOperand::constInt(0), TACOperand::none()});
    }
    // 局部变量无初始化：不发射指令（CodeGen 按需分配栈空间）
}

void IRGenerator::visit(IfStmt& n) {
    // if-else 控制流
    //   L_then: then 分支入口
    //   L_else: else 分支入口（如有）
    //   L_end:  结束标签
    //
    //   条件求值 → if result goto L_then
    //   goto L_else (或 L_end，若无 else)
    // L_then:
    //   thenBranch
    //   goto L_end
    // L_else:  (如有)
    //   elseBranch
    // L_end:

    auto thenLabel = newLabel("L_then");
    auto endLabel  = newLabel("L_end");

    // 求值条件
    n.condition->accept(*this);
    std::string condResult = result_;

    if (n.elseBranch) {
        auto elseLabel = newLabel("L_else");

        // if condResult != 0 goto L_then
        emit(TACInstruction{TACType::IF_GOTO, TACOperand::none(),
            TACOperand::var(condResult), TACOperand::none(), "", thenLabel});
        // else: goto L_else
        emit(TACInstruction{TACType::GOTO, TACOperand::none(), TACOperand::none(),
            TACOperand::none(), "", elseLabel});

        // L_then:
        emit(TACInstruction{TACType::LABEL, TACOperand::none(), TACOperand::none(),
            TACOperand::none(), "", thenLabel});
        n.thenBranch->accept(*this);
        emit(TACInstruction{TACType::GOTO, TACOperand::none(), TACOperand::none(),
            TACOperand::none(), "", endLabel});

        // L_else:
        emit(TACInstruction{TACType::LABEL, TACOperand::none(), TACOperand::none(),
            TACOperand::none(), "", elseLabel});
        n.elseBranch->accept(*this);
        // 自然落入 L_end
    } else {
        // 无 else 分支
        // if condResult != 0 goto L_then
        emit(TACInstruction{TACType::IF_GOTO, TACOperand::none(),
            TACOperand::var(condResult), TACOperand::none(), "", thenLabel});
        // else: goto L_end
        emit(TACInstruction{TACType::GOTO, TACOperand::none(), TACOperand::none(),
            TACOperand::none(), "", endLabel});

        // L_then:
        emit(TACInstruction{TACType::LABEL, TACOperand::none(), TACOperand::none(),
            TACOperand::none(), "", thenLabel});
        n.thenBranch->accept(*this);
    }

    // L_end:
    emit(TACInstruction{TACType::LABEL, TACOperand::none(), TACOperand::none(),
        TACOperand::none(), "", endLabel});
}

void IRGenerator::visit(WhileStmt& n) {
    // while 循环控制流
    // L_start:
    //   条件求值 → if result goto L_body
    //   goto L_end
    // L_body:
    //   body
    //   goto L_start
    // L_end:

    auto startLabel = newLabel("L_start");
    auto bodyLabel  = newLabel("L_body");
    auto endLabel   = newLabel("L_end");

    // L_start:
    emit(TACInstruction{TACType::LABEL, TACOperand::none(), TACOperand::none(),
        TACOperand::none(), "", startLabel});

    // 条件求值
    n.condition->accept(*this);
    std::string condResult = result_;

    // if condResult != 0 goto L_body
    emit(TACInstruction{TACType::IF_GOTO, TACOperand::none(),
        TACOperand::var(condResult), TACOperand::none(), "", bodyLabel});
    // 条件为假 → goto L_end
    emit(TACInstruction{TACType::GOTO, TACOperand::none(), TACOperand::none(),
        TACOperand::none(), "", endLabel});

    // L_body:
    emit(TACInstruction{TACType::LABEL, TACOperand::none(), TACOperand::none(),
        TACOperand::none(), "", bodyLabel});

    // 压入 break/continue 标签 (嵌套循环支持)
    loopLabels_.push({endLabel, startLabel});

    n.body->accept(*this);

    // 弹出标签
    loopLabels_.pop();

    // 回到循环开头
    emit(TACInstruction{TACType::GOTO, TACOperand::none(), TACOperand::none(),
        TACOperand::none(), "", startLabel});

    // L_end:
    emit(TACInstruction{TACType::LABEL, TACOperand::none(), TACOperand::none(),
        TACOperand::none(), "", endLabel});
}

void IRGenerator::visit(BlockStmt& n) {
    enterScope();
    for (auto& s : n.statements) {
        s->accept(*this);
    }
    exitScope();
}

void IRGenerator::visit(BreakStmt& /*n*/) {
    // 跳转到当前循环的 end 标签
    if (!loopLabels_.empty()) {
        emit(TACInstruction{TACType::GOTO, TACOperand::none(), TACOperand::none(),
            TACOperand::none(), "", loopLabels_.top().first});
    }
}

void IRGenerator::visit(ContinueStmt& /*n*/) {
    // 跳转到当前循环的 start 标签
    if (!loopLabels_.empty()) {
        emit(TACInstruction{TACType::GOTO, TACOperand::none(), TACOperand::none(),
            TACOperand::none(), "", loopLabels_.top().second});
    }
}

void IRGenerator::visit(ReturnStmt& n) {
    // return expr; 或 return;
    if (n.value) {
        n.value->accept(*this);
        emit(TACInstruction{TACType::RETURN, TACOperand::none(),
            TACOperand::var(result_), TACOperand::none()});
    } else {
        emit(TACInstruction{TACType::RETURN, TACOperand::none(),
            TACOperand::none(), TACOperand::none()});
    }
}

void IRGenerator::visit(FunctionDecl& n) {
    currentFuncReturnType_ = n.returnType;

    std::string funcLabel = "func_" + n.name;
    emit(TACInstruction{TACType::LABEL, TACOperand::none(), TACOperand::none(),
        TACOperand::none(), "", funcLabel});

    // 进入函数作用域并注册参数
    enterScope();
    for (auto& param : n.params) {
        std::string uniqueName = registerVar(param.first);
        emit(TACInstruction{TACType::FUNC_ARG, TACOperand::var(uniqueName),
            TACOperand::none(), TACOperand::none()});
    }

    n.body->accept(*this);
    exitScope();

    // 补默认 return
    if (n.returnType == DataType::VOID) {
        emit(TACInstruction{TACType::RETURN, TACOperand::none(),
            TACOperand::none(), TACOperand::none()});
    }
    if (n.returnType == DataType::INT) {
        emit(TACInstruction{TACType::RETURN, TACOperand::none(),
            TACOperand::constInt(0), TACOperand::none()});
    }
}

void IRGenerator::visit(Program& n) {
    // 初始化全局作用域
    scopeMaps_.clear();
    scopeMaps_.push_back({});  // 全局作用域 (scope 0)
    varCounter_ = 0;

    // 处理全局变量/常量声明 → 生成 ASSIGN 指令
    for (auto& s : n.statements) {
        s->accept(*this);
    }

    // 直接生成所有函数定义（无需 GOTO 跳过，crt0.o 直接调用 main）
    for (auto& f : n.functions) {
        f->accept(*this);
    }
}

} // namespace MyCompiler
