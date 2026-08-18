/// @file CSE.cpp
/// @brief 公共子表达式消除 (Common Subexpression Elimination)
///
/// 若同一表达式在同一基本块内被重复计算，则复用第一次的结果。
/// 例如:
///   t0 = a + b
///   t1 = a + b      →  t1 = t0
///
/// 变量被重新赋值时，涉及该变量的缓存条目失效。

#include "Optimizer.h"
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

namespace MyCompiler {

/// 为 BINARY 指令生成唯一键: "op|lhsStr|rhsStr"
static std::string makeBinaryKey(const TACInstruction& instr) {
    return instr.op + "|" + instr.lhs.toString() + "|" + instr.rhs.toString();
}

/// 为 UNARY 指令生成唯一键: "op|lhsStr"
static std::string makeUnaryKey(const TACInstruction& instr) {
    return instr.op + "|" + instr.lhs.toString();
}

/// 从缓存中删除所有包含指定变量名的条目
static void invalidateVar(std::unordered_map<std::string, std::string>& cache,
                          const std::string& varName) {
    // 遍历并删除 key 中包含 varName 的条目
    // 注意：不能边遍历边删除，先收集再删
    std::vector<std::string> toRemove;
    for (auto& kv : cache) {
        // varName 可能作为完整 token 出现在 key 中
        // 简单策略：key 中包含该 varName 就失效
        if (kv.first.find(varName) != std::string::npos) {
            toRemove.push_back(kv.first);
        }
    }
    for (auto& key : toRemove) {
        cache.erase(key);
    }
}

void Optimizer::commonSubexpressionElimination(TACProgram& program) {
    std::unordered_map<std::string, std::string> cache;
    // 收集被跳转引用的标签（只有这些清缓存，避免过度失效）
    std::unordered_set<std::string> jumpTargets;
    for (auto& instr : program.instructions) {
        if ((instr.type == TACType::GOTO || instr.type == TACType::IF_GOTO) && !instr.label.empty())
            jumpTargets.insert(instr.label);
    }

    int eliminated = 0;
    for (auto& instr : program.instructions) {
        // --- 只在跳转目标处重置缓存 ---
        if (instr.type == TACType::LABEL && jumpTargets.count(instr.label)) {
            cache.clear();
            continue;
        }

        // --- 变量赋值：使涉及该变量的缓存失效 ---
        if (instr.type == TACType::ASSIGN &&
            (instr.result.type == TACOpType::VAR || instr.result.type == TACOpType::TEMP)) {
            invalidateVar(cache, instr.result.name);
        }
        if ((instr.type == TACType::BINARY || instr.type == TACType::UNARY ||
             instr.type == TACType::FUNC_ARG) && !instr.result.name.empty()) {
            invalidateVar(cache, instr.result.name);
        }

        // --- BINARY CSE ---
        if (instr.type == TACType::BINARY &&
            instr.lhs.type != TACOpType::CONST_INT && instr.rhs.type != TACOpType::CONST_INT) {
            std::string key = makeBinaryKey(instr);
            auto it = cache.find(key);
            if (it != cache.end()) {
                instr.type = TACType::ASSIGN;
                instr.lhs = TACOperand::var(it->second);
                instr.rhs = TACOperand::none(); instr.op.clear();
                ++eliminated;
            } else if (!instr.result.name.empty()) {
                cache[key] = instr.result.name;
            }
        }

        // --- UNARY CSE ---
        if (instr.type == TACType::UNARY &&
            instr.lhs.type != TACOpType::CONST_INT) {
            std::string key = makeUnaryKey(instr);
            auto it = cache.find(key);
            if (it != cache.end()) {
                instr.type = TACType::ASSIGN;
                instr.lhs = TACOperand::var(it->second);
                instr.op.clear();
                ++eliminated;
            } else if (!instr.result.name.empty()) {
                cache[key] = instr.result.name;
            }
        }

        // --- ASSIGN 传播：x = y 后，后续用 y 替换为 x 的缓存 ---
        if (instr.type == TACType::ASSIGN &&
            instr.lhs.type != TACOpType::CONST_INT &&
            (instr.result.type == TACOpType::VAR || instr.result.type == TACOpType::TEMP)) {
            std::string srcKey = "CP_" + instr.lhs.name;
            cache[srcKey] = instr.result.name;
        }
    }

    if (eliminated > 0)
        std::cerr << "[CSE] 消除: " << eliminated << " 条\n";
}

} // namespace MyCompiler
