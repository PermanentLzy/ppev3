#include "Optimizer.h"
#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace MyCompiler {

// ================================================================
//  循环不变式提取（Loop Invariant Code Motion - LICM）
//
//  正确性条件（一条指令可被安全地提到循环外）：
//    1. 指令是 "纯计算" (ASSIGN / BINARY / UNARY)，无副作用
//    2. 所有操作数都是 "循环不变"：
//       - 常数 (CONST_INT / NONE)
//       - 在循环外定义 且 在循环体内没有被重新定义
//       - 由本趟已识别的循环不变式定义（链式）
//    3. 指令的 result 在循环体内只被定义一次（避免覆盖）
//    4. 指令不能抛异常 / 不能是除零等可能失败的操作（保守起见，包含 div/rem 时跳过）
// ================================================================

/// 判断操作数是否为常数类（CONST_INT 或 NONE）
static bool isConstLike(const TACOperand& op) {
    return op.type == TACOpType::CONST_INT || op.type == TACOpType::NONE;
}

/// 收集指令使用到的操作数（lhs、rhs，但不包含 result）
static std::vector<TACOperand> getUsedOperands(const TACInstruction& instr) {
    std::vector<TACOperand> ops;
    if (instr.lhs.type != TACOpType::NONE) ops.push_back(instr.lhs);
    if (instr.rhs.type != TACOpType::NONE) ops.push_back(instr.rhs);
    return ops;
}

/// 判断变量名是否定义在某条指令的 result 中
static bool instrDefines(const TACInstruction& instr, const std::string& name) {
    if (name.empty()) return false;
    if (instr.result.type != TACOpType::VAR && instr.result.type != TACOpType::TEMP)
        return false;
    return instr.result.name == name;
}

void Optimizer::loopInvariantCodeMotion(TACProgram& program) {
    std::vector<TACInstruction>& instrs = program.instructions;
    if (instrs.empty()) return;

    int totalHoisted = 0;

    // ---- 第 2 遍：对每个循环做不变式提取 ----
    // 注意：每次外提后指令数组会重建，索引会变化。
    // 因此我们逐个处理循环：每次重新在当前 instrs 中定位循环，
    // 处理完后重新扫描，避免使用过时的索引。
    // 用 processedLabels 记录已处理（无可外提不变式）的循环头标签，
    // 避免重复扫描同一个循环导致死循环。
    std::unordered_set<std::string> processedLabels;
    bool didWork = true;
    while (didWork) {
        didWork = false;

        // 重新扫描所有循环（LABEL → ... → GOTO LABEL）
        // 跳过已处理的循环，找到下一个未处理的循环
        int head = -1, bodyStart = -1, bodyEnd = -1;
        std::string loopLabel;
        for (size_t i = 0; i < instrs.size(); ++i) {
            if (instrs[i].type != TACType::LABEL) continue;
            const std::string& lbl = instrs[i].label;
            if (processedLabels.count(lbl)) continue;  // 跳过已处理
            // 在 i 之后找一条 GOTO 跳回 lbl
            for (size_t j = i + 1; j < instrs.size(); ++j) {
                if (instrs[j].type == TACType::GOTO && instrs[j].label == lbl) {
                    head = static_cast<int>(i);
                    bodyStart = static_cast<int>(i) + 1;
                    bodyEnd = static_cast<int>(j);
                    loopLabel = lbl;
                    break;
                }
            }
            if (head >= 0) break;
        }

        if (head < 0) break;  // 没有更多循环

        // 收集循环体内（bodyStart..bodyEnd）所有定义的变量名
        std::unordered_set<std::string> definedInLoop;
        std::unordered_map<std::string, int> defCount;
        for (int idx = bodyStart; idx < bodyEnd; ++idx) {
            const auto& instr = instrs[idx];
            if (instr.result.type == TACOpType::VAR || instr.result.type == TACOpType::TEMP) {
                if (!instr.result.name.empty()) {
                    definedInLoop.insert(instr.result.name);
                    defCount[instr.result.name]++;
                }
            }
        }

        // 循环前已定义的变量名
        std::unordered_set<std::string> preLoopDef;
        for (int i = 0; i < head; ++i) {
            const auto& instr = instrs[i];
            if (instr.result.type == TACOpType::VAR || instr.result.type == TACOpType::TEMP) {
                if (!instr.result.name.empty()) preLoopDef.insert(instr.result.name);
            }
        }

        // 迭代找不变式（可能存在链式依赖：t1 = a+b; t2 = t1*c;）
        std::vector<int> invariantIndices;
        std::unordered_set<std::string> invariantVars;

        bool changed = true;
        int maxIter = 10;
        while (changed && maxIter-- > 0) {
            changed = false;
            for (int idx = bodyStart; idx < bodyEnd; ++idx) {
                if (std::find(invariantIndices.begin(), invariantIndices.end(), idx) != invariantIndices.end())
                    continue;

                const auto& instr = instrs[idx];

                // 仅处理纯计算
                if (instr.type != TACType::ASSIGN &&
                    instr.type != TACType::BINARY &&
                    instr.type != TACType::UNARY)
                    continue;

                // 保守：包含除法/求模的指令不外提（避免除零行为变化）
                if (instr.type == TACType::BINARY &&
                    (instr.op == "/" || instr.op == "%")) {
                    if (instr.rhs.type != TACOpType::CONST_INT || instr.rhs.intValue == 0)
                        continue;
                }

                // result 必须在循环内只被定义一次
                if (instr.result.type == TACOpType::VAR || instr.result.type == TACOpType::TEMP) {
                    if (defCount[instr.result.name] > 1) continue;
                }

                // 所有操作数必须是不变量
                auto usedOps = getUsedOperands(instr);
                bool allInvariant = true;
                for (const auto& op : usedOps) {
                    if (isConstLike(op)) continue;
                    if (op.type != TACOpType::VAR && op.type != TACOpType::TEMP) {
                        allInvariant = false; break;
                    }
                    bool isInvariant =
                        (preLoopDef.count(op.name) && !definedInLoop.count(op.name)) ||
                        invariantVars.count(op.name);
                    if (!isInvariant) { allInvariant = false; break; }
                }
                if (!allInvariant) continue;

                invariantIndices.push_back(idx);
                if (instr.result.type == TACOpType::VAR || instr.result.type == TACOpType::TEMP) {
                    invariantVars.insert(instr.result.name);
                }
                changed = true;
            }
        }

        if (invariantIndices.empty()) {
            // 这个循环没有可外提的不变式，标记为已处理，继续找下一个循环
            processedLabels.insert(loopLabel);
            didWork = true;
            continue;
        }

        // 把这些不变式从原位置删除，插入到循环 head 之前
        std::sort(invariantIndices.begin(), invariantIndices.end());

        std::vector<TACInstruction> newInstrs;
        std::unordered_set<int> toRemove(invariantIndices.begin(), invariantIndices.end());

        for (size_t i = 0; i < instrs.size(); ++i) {
            if (static_cast<int>(i) == head) {
                // 在 LABEL 之前插入不变式
                for (int invIdx : invariantIndices)
                    newInstrs.push_back(instrs[invIdx]);
            }
            if (toRemove.find(static_cast<int>(i)) != toRemove.end())
                continue; // 跳过已外提的指令
            newInstrs.push_back(instrs[i]);
        }

        instrs = std::move(newInstrs);
        totalHoisted += static_cast<int>(invariantIndices.size());
        processedLabels.insert(loopLabel);  // 标记此循环已处理
        didWork = true;
        // 循环回到 while 顶部，重新扫描所有循环
        // 这样后续循环的索引会基于重建后的数组，不会过时
    }

    if (totalHoisted > 0)
        std::cerr << "[LICM] 外提 " << totalHoisted << " 条不变式\n";
    else
        std::cerr << "[LICM] 未发现可外提不变式\n";
}

} // namespace MyCompiler
