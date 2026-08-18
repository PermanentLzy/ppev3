/// @file CopyProp.cpp
/// @brief 复写传播 (Copy Propagation)
///
/// 将 x = y 形式的复制指令传播到后续使用处。
/// 例如:
///   x = y
///   z = x + 1    →  z = y + 1
///   如果 x 不再被使用，则可删除 x = y
///
/// 与 DCE 联动: 传播后若 x 无其他使用，由 DCE 删除

#include "Optimizer.h"
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

namespace MyCompiler {

/// 获取操作数名称
static std::string opn(const TACOperand& op) {
    if (op.type == TACOpType::VAR || op.type == TACOpType::TEMP)
        return op.name;
    return "";
}

/// 替换指令中的操作数
static void replaceInInstr(TACInstruction& instr, const std::string& from, const std::string& to) {
    if (opn(instr.lhs) == from) instr.lhs = TACOperand::var(to);
    if (opn(instr.rhs) == from) instr.rhs = TACOperand::var(to);
}

void Optimizer::copyPropagation(TACProgram& program) {
    // 收集使用计数：哪些变量被引用了
    std::unordered_map<std::string, int> useCount;
    for (auto& instr : program.instructions) {
        std::string l = opn(instr.lhs), r = opn(instr.rhs);
        if (!l.empty()) useCount[l]++;
        if (!r.empty()) useCount[r]++;
    }

    int propagated = 0;
    for (size_t i = 0; i < program.instructions.size(); ++i) {
        auto& instr = program.instructions[i];

        // 识别 x = y (y 是 VAR/TEMP, x 也是 VAR/TEMP)
        if (instr.type == TACType::ASSIGN &&
            (instr.lhs.type == TACOpType::VAR || instr.lhs.type == TACOpType::TEMP) &&
            (instr.result.type == TACOpType::VAR || instr.result.type == TACOpType::TEMP) &&
            instr.lhs.type != TACOpType::CONST_INT) {

            std::string from = instr.result.name;
            std::string to = instr.lhs.name;

            // 只在复写安全时传播（源变量在传播区间内不被修改）
            if (!from.empty() && !to.empty() && from != to &&
                !from.empty() && !to.empty()) {

                // 扫描后续指令，替换对 from 的使用
                bool modified = false;
                for (size_t j = i + 1; j < program.instructions.size(); ++j) {
                    auto& later = program.instructions[j];

                    // 遇到可能修改变量的指令，停止传播
                    if (later.type == TACType::CALL || later.type == TACType::PARAM ||
                        later.type == TACType::FUNC_ARG || later.type == TACType::RETURN) break;
                    // to 被重新赋值，停止
                    if ((later.type == TACType::ASSIGN || later.type == TACType::BINARY ||
                         later.type == TACType::UNARY) && opn(later.result) == to) break;
                    // from 被重新赋值，停止
                    if ((later.type == TACType::ASSIGN || later.type == TACType::BINARY ||
                         later.type == TACType::UNARY) && opn(later.result) == from) break;

                    // 替换
                    auto lo = opn(later.lhs), ro = opn(later.rhs);
                    std::string nl = (lo == from) ? to : lo;
                    std::string nr = (ro == from) ? to : ro;
                    if (nl != lo || nr != ro) {
                        if (nl != lo) later.lhs = TACOperand::var(nl);
                        if (nr != ro) later.rhs = TACOperand::var(nr);
                        modified = true;
                    }
                }
                if (modified) ++propagated;
            }
        }
    }

    if (propagated > 0)
        std::cerr << "[CopyProp] 复写传播: " << propagated << " 处\n";
}

} // namespace MyCompiler
