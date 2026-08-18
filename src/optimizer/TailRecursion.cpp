/// @file TailRecursion.cpp
/// @brief 尾递归消除 —— 参数副本方案
///
/// 核心思路：
///   1. FUNC_ARG 后创建副本变量 tco_f_p0 = param0, tco_f_p1 = param1
///   2. 插入循环头标签 L_tco_f
///   3. 函数体内所有形参引用 → 副本变量
///   4. 尾调用点：
///      - 保留计算指令（arg 值已在 temp 中）
///      - 跳过 PARAM/CALL
///      - RETURN 位置：先输出所有 tco_f_pN = argN，再 goto L_tco_f
///      保证 "先计算所有参数值，再统一赋给副本" 的正确语义

#include "Optimizer.h"
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

namespace MyCompiler {

void Optimizer::tailRecursionElimination(TACProgram& program) {
    auto& insts = program.instructions;
    if (insts.empty()) return;

    auto varName = [](const TACOperand& op) -> std::string {
        if (op.type == TACOpType::VAR || op.type == TACOpType::TEMP) return op.name;
        return "";
    };

    // ======== 第1遍：收集函数参数 ========
    std::unordered_map<std::string, std::vector<std::string>> funcParams;
    std::string curFn;
    for (auto& inst : insts) {
        if (inst.type == TACType::LABEL && inst.label.find("func_") == 0) {
            curFn = inst.label.substr(5);
            funcParams[curFn] = {};
        }
        if (inst.type == TACType::FUNC_ARG && !curFn.empty()) {
            std::string p = varName(inst.result);
            if (!p.empty()) funcParams[curFn].push_back(p);
        }
    }
    if (funcParams.empty()) return;

    // ======== 第2遍：检测尾调用点 ========
    struct TailSite {
        size_t callIdx;
        std::string fn;
        std::vector<size_t> paramIdxs;  // PARAM索引（左→右）
        std::vector<std::string> argTemps; // arg 的 temp 名
    };
    std::vector<TailSite> sites;
    curFn.clear();

    for (size_t i = 0; i < insts.size(); ++i) {
        if (insts[i].type == TACType::LABEL && insts[i].label.find("func_") == 0)
            curFn = insts[i].label.substr(5);

        if (insts[i].type == TACType::CALL && !curFn.empty() &&
            insts[i].label == curFn &&
            i + 1 < insts.size() && insts[i + 1].type == TACType::RETURN) {

            auto it = funcParams.find(curFn);
            if (it == funcParams.end()) continue;
            size_t np = it->second.size();
            if (np == 0) continue;

            // 从 CALL 向前扫 PARAM，跳过计算指令
            std::vector<size_t> rev;
            for (size_t j = i; j > 0; --j) {
                auto t = insts[j - 1].type;
                if (t == TACType::PARAM) {
                    rev.push_back(j - 1);
                    if (rev.size() >= np) break;
                } else if (t == TACType::LABEL || t == TACType::GOTO ||
                           t == TACType::IF_GOTO || t == TACType::RETURN ||
                           t == TACType::CALL || t == TACType::FUNC_ARG) {
                    break;
                }
            }

            if (rev.size() == np) {
                std::reverse(rev.begin(), rev.end());
                TailSite ts;
                ts.callIdx = i;
                ts.fn = curFn;
                ts.paramIdxs = rev;
                for (auto pi : rev) ts.argTemps.push_back(varName(insts[pi].lhs));
                sites.push_back(ts);
            }
        }
    }

    if (sites.empty()) { std::cerr << "[TCO] 未发现尾调用\n"; return; }

    // ======== 第3遍：构建映射 ========
    // paramName → "tco_fn_p0"
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> copyMap;
    std::unordered_map<std::string, std::string> loopLabel;
    for (auto& s : sites) {
        if (copyMap.count(s.fn)) continue;
        auto& params = funcParams[s.fn];
        auto& m = copyMap[s.fn];
        for (size_t k = 0; k < params.size(); ++k)
            m[params[k]] = "tco_" + s.fn + "_p" + std::to_string(k);
        loopLabel[s.fn] = "L_tco_" + s.fn;
    }

    // PARAM索引 → 哪个尾调用点
    std::unordered_map<size_t, size_t> p2call;
    for (auto& s : sites)
        for (auto pi : s.paramIdxs) p2call[pi] = s.callIdx;

    // callIdx → TailSite*
    std::unordered_map<size_t, TailSite*> callMap;
    for (auto& s : sites) callMap[s.callIdx] = &s;

    // ======== 第4遍：生成输出 ========
    std::vector<TACInstruction> out;
    out.reserve(insts.size() + sites.size() * 5);

    curFn.clear();
    bool afterArgs = false, inserted = false;
    std::string lbl;

    for (size_t i = 0; i < insts.size(); ++i) {
        auto instr = insts[i];

        // 跟踪函数
        if (instr.type == TACType::LABEL && instr.label.find("func_") == 0) {
            curFn = instr.label.substr(5);
            afterArgs = false; inserted = false;
            lbl = loopLabel.count(curFn) ? loopLabel[curFn] : "";
        }
        if (instr.type == TACType::FUNC_ARG) afterArgs = true;

        // PARAM（尾调用）→ 跳过
        if (p2call.count(i)) continue;

        // CALL（尾调用）→ 跳过
        if (callMap.count(i)) continue;

        // RETURN（紧接尾调用的CALL）→ 输出副本赋值 + GOTO
        if (i > 0 && callMap.count(i - 1)) {
            auto* site = callMap[i - 1];
            auto& m = copyMap[site->fn];
            auto& params = funcParams[site->fn];
            auto& args = site->argTemps;

            for (size_t k = 0; k < params.size() && k < args.size(); ++k) {
                TACInstruction a;
                a.type = TACType::ASSIGN;
                a.result = TACOperand::temp(m[params[k]]);
                a.lhs = TACOperand::temp(args[k]);
                out.push_back(a);
            }

            TACInstruction jmp;
            jmp.type = TACType::GOTO;
            jmp.label = lbl;
            out.push_back(jmp);
            continue;
        }

        // 在FUNC_ARG后插入副本+循环标签
        if (afterArgs && !inserted && instr.type != TACType::FUNC_ARG &&
            !curFn.empty() && copyMap.count(curFn)) {
            auto& params = funcParams[curFn];
            auto& m = copyMap[curFn];
            for (size_t k = 0; k < params.size(); ++k) {
                TACInstruction cp;
                cp.type = TACType::ASSIGN;
                cp.result = TACOperand::temp(m[params[k]]);
                cp.lhs = TACOperand::var(params[k]);
                out.push_back(cp);
            }
            TACInstruction lb;
            lb.type = TACType::LABEL;
            lb.label = lbl;
            out.push_back(lb);
            inserted = true;
        }

        // 替换形参→副本
        if (!curFn.empty() && copyMap.count(curFn) && inserted) {
            auto& m = copyMap[curFn];
            std::string ln = varName(instr.lhs);
            if (!ln.empty() && m.count(ln)) instr.lhs = TACOperand::temp(m[ln]);
            std::string rn = varName(instr.rhs);
            if (!rn.empty() && m.count(rn)) instr.rhs = TACOperand::temp(m[rn]);
        }

        out.push_back(std::move(instr));
    }

    program.instructions = std::move(out);
    std::cerr << "[TCO] 消除尾调用: " << sites.size() << " 处\n";
}

} // namespace MyCompiler
