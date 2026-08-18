/// @file DeadCodeElim.cpp
/// @brief 死代码删除 (Dead Code Elimination)
///
/// 基于逆向活跃变量分析 (backward liveness analysis)：
///   1. 标记所有有副作用的指令为「必需」(RETURN / CALL / IF_GOTO / GOTO / LABEL)
///   2. 从必需指令的操作数出发，逆向标记活跃变量
///   3. 若某指令的结果变量是活跃的，则该指令也被标记为必需，
///      并将其操作数加入活跃集，反复迭代直到不动点
///   4. 扫除阶段：移除所有未被标记的指令
///
/// 示例:
///   x = 1        ← x 从未被使用 → 删除
///   y = 2        ← y 被 return 使用 → 保留
///   return y

#include "Optimizer.h"
#include <iostream>
#include <vector>
#include <unordered_set>
#include <string>

namespace MyCompiler {

/// 判断指令是否具有副作用，不可删除
static bool hasSideEffect(const TACInstruction& instr) {
    switch (instr.type) {
        case TACType::GOTO:
        case TACType::IF_GOTO:
        case TACType::LABEL:
        case TACType::CALL:
        case TACType::PARAM:
        case TACType::FUNC_ARG:
        case TACType::RETURN:
            return true;
        default:
            return false;
    }
}

/// 获取操作数的变量名（仅 VAR / TEMP 有名称，CONST_INT 和 NONE 返回空串）
static std::string operandName(const TACOperand& op) {
    if (op.type == TACOpType::VAR || op.type == TACOpType::TEMP) {
        return op.name;
    }
    return "";
}

/// 获取指令的「定值」变量名（result），若无则返回空串
static std::string defName(const TACInstruction& instr) {
    return operandName(instr.result);
}

/// 收集指令中所有被引用的操作数名称（lhs, rhs）
static void collectUses(const TACInstruction& instr,
                        std::unordered_set<std::string>& uses) {
    std::string l = operandName(instr.lhs);
    std::string r = operandName(instr.rhs);
    if (!l.empty()) uses.insert(l);
    if (!r.empty()) uses.insert(r);
}

void Optimizer::deadCodeElimination(TACProgram& program) {
    const size_t n = program.instructions.size();
    if (n == 0) return;

    // ---- 第 1 步：标记有副作用的指令为「必需」----
    std::vector<bool> needed(n, false);
    for (size_t i = 0; i < n; ++i) {
        if (hasSideEffect(program.instructions[i])) {
            needed[i] = true;
        }
    }

    // ---- 第 2 步：活跃变量集合，初始从必需指令的操作数开始 ----
    std::unordered_set<std::string> live;
    for (size_t i = 0; i < n; ++i) {
        if (needed[i]) {
            collectUses(program.instructions[i], live);
        }
    }

    // ---- 第 3 步：逆向迭代直到不动点 ----
    bool changed = true;
    while (changed) {
        changed = false;
        // 逆向扫描指令
        for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
            std::string def = defName(program.instructions[i]);

            // 若该指令定义了某个活跃变量，则该指令必需
            if (!def.empty() && live.count(def) > 0) {
                if (!needed[i]) {
                    needed[i] = true;
                    changed = true;
                }
                // 将其操作数也加入活跃集
                size_t before = live.size();
                collectUses(program.instructions[i], live);
                if (live.size() > before) {
                    changed = true;
                }
            }

            // 若该指令原本就是必需的（副作用），也要传播其操作数到活跃集
            if (needed[i]) {
                size_t before = live.size();
                collectUses(program.instructions[i], live);
                if (live.size() > before) {
                    changed = true;
                }
            }
        }
    }

    // ---- 第 4 步：扫除 —— 保留标记的指令 ----
    std::vector<TACInstruction> kept;
    int removed = 0;
    for (size_t i = 0; i < n; ++i) {
        if (needed[i]) {
            kept.push_back(std::move(program.instructions[i]));
        } else {
            ++removed;
        }
    }
    program.instructions = std::move(kept);

    if (removed > 0) {
        std::cerr << "[DCE] 死代码删除: " << removed << " 条指令\n";
    }
}

} // namespace MyCompiler
