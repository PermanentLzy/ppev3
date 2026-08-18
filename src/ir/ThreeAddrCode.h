#pragma once

#include <string>
#include <vector>

namespace MyCompiler {

/// @brief 三地址码操作数类型
enum class TACOpType { TEMP, VAR, CONST_INT, NONE };

/// @brief 三地址码操作数
struct TACOperand {
    TACOpType type = TACOpType::NONE;
    int intValue = 0;
    std::string name;       // TEMP / VAR 的名称

    static TACOperand temp(const std::string& name) {
        TACOperand op;
        op.type = TACOpType::TEMP;
        op.name = name;
        return op;
    }
    static TACOperand var(const std::string& name) {
        TACOperand op;
        op.type = TACOpType::VAR;
        op.name = name;
        return op;
    }
    static TACOperand constInt(int v) {
        TACOperand op;
        op.type = TACOpType::CONST_INT;
        op.intValue = v;
        return op;
    }
    static TACOperand none() {
        TACOperand op;
        op.type = TACOpType::NONE;
        return op;
    }

    std::string toString() const;
};

/// @brief 三地址码指令类型
enum class TACType {
    BINARY,     // x = y op z
    UNARY,      // x = op y
    ASSIGN,     // x = y
    PARAM,      // param x      (函数调用参数传递，调用方)
    FUNC_ARG,   // arg x        (函数参数接收，被调方：x = 第N个参数寄存器)
    GOTO,       // goto L
    IF_GOTO,    // if x goto L
    LABEL,      // L:
    CALL,       // call f, n
    RETURN,     // return x
    NOP
};

/// @brief 三地址码指令
struct TACInstruction {
    TACType type;
    TACOperand result;
    TACOperand lhs;
    TACOperand rhs;
    std::string op;   // 运算符字符串，如 "+", "-", "<" 等
    std::string label;// 标签名

    std::string toString() const;
};

/// @brief 三地址码序列
struct TACProgram {
    std::vector<TACInstruction> instructions;

    /// 生成新临时变量名
    std::string newTemp() {
        return "t" + std::to_string(tempCounter_++);
    }

    /// 生成新标签名
    std::string newLabel(const std::string& prefix = "L") {
        return prefix + std::to_string(labelCounter_++);
    }

    /// 添加指令
    void emit(const TACInstruction& instr) {
        instructions.push_back(instr);
    }

    void print() const;

private:
    int tempCounter_ = 0;
    int labelCounter_ = 0;
};

} // namespace MyCompiler
