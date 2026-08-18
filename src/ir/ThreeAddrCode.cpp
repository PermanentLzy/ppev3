#include "ThreeAddrCode.h"
#include <iostream>

namespace MyCompiler {

std::string TACOperand::toString() const {
    // TODO: 根据 type 返回对应的字符串表示
    switch (type) {
        case TACOpType::TEMP:       return name;
        case TACOpType::VAR:        return name;
        case TACOpType::CONST_INT:  return std::to_string(intValue);
        default:                    return "_";
    }
    return "_";
}

std::string TACInstruction::toString() const {
    // TODO: 根据 type 生成可读的指令字符串
    //   BINARY:  "result = lhs op rhs"
    //   UNARY:   "result = op lhs"
    //   ASSIGN:  "result = lhs"
    //   GOTO:    "goto label"
    //   IF_GOTO: "if lhs goto label"
    //   LABEL:   "label:"
    switch (type) {
        case TACType::BINARY:
            return result.toString() + " = " + lhs.toString() + " " + op + " " + rhs.toString();
        case TACType::UNARY:
            return result.toString() + " = " + op + " " + lhs.toString();
        case TACType::ASSIGN:
            return result.toString() + " = " + lhs.toString();
        case TACType::PARAM:
            return "param " + lhs.toString();
        case TACType::FUNC_ARG:
            return "arg " + result.toString();
        case TACType::GOTO:
            return "goto " + label;
        case TACType::IF_GOTO:
            return "if " + lhs.toString() + " goto " + label;
        case TACType::LABEL:
            return label + ":";
        case TACType::CALL:
            // TODO: "call f, n" 格式，label 存函数名，lhs 存参数信息
            return "call " + label;
        case TACType::RETURN:
            // TODO: "return x" 或 "return"
            if (lhs.type != TACOpType::NONE)
                return "return " + lhs.toString();
            return "return";
        case TACType::NOP:
            return "";
    }
    return "nop";
}

void TACProgram::print() const {
    // DEBUG: print to stderr so stdout stays clean for assembly output
    for (size_t i = 0 ; i < instructions.size(); ++i) {
        std::cerr << i << ": " << instructions[i].toString() << "\n";
    }
}

} // namespace MyCompiler
