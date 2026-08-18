#include "Optimizer.h"
#include <iostream>
#include <unordered_set>
#include <string>
#include <algorithm>

namespace MyCompiler
{

    // ================================================================
    //  主入口：串联所有优化 pass，迭代直到 IR 不再变化
    // ================================================================
    void Optimizer::optimize(TACProgram &program)
    {
        // ---- 第 0 轮：一次性优化（只执行一次）----
        tailRecursionElimination(program); // 尾递归消除
        strengthReduction(program);        // 强度削弱
        loopInvariantCodeMotion(program);  // 循环不变式提取

        bool changed = true;
        int iter = 0;
        const int MAX_ITER = 20;

        while (changed && iter < MAX_ITER)
        {
            changed = false;
            size_t before = program.instructions.size();

            // 对每条指令计算签名（toString 哈希），用于检测内容变化
            std::vector<std::string> beforeSigs;
            beforeSigs.reserve(program.instructions.size());
            for (auto &instr : program.instructions)
                beforeSigs.push_back(instr.toString());

            constantFolding(program);                // 常量折叠
            copyPropagation(program);                 // 复写传播
            commonSubexpressionElimination(program); // 公共子表达式消除（CSE）
            deadCodeElimination(program);            // 死代码删除（DCE）
            removeUnreachableCode(program);          // 不可达代码删除
            coalesceTemporaryCopies(program);        // 临时→局部合并
            peepholeOptimization(program);           // 窥孔优化

            // 检查指令数量变化
            if (program.instructions.size() != before)
                changed = true;

            // 检查指令内容变化（即使数量不变，内容变化也需要继续迭代）
            if (!changed)
            {
                size_t minLen = std::min(beforeSigs.size(), program.instructions.size());
                for (size_t i = 0; i < minLen; ++i)
                {
                    if (beforeSigs[i] != program.instructions[i].toString())
                    {
                        changed = true;
                        break;
                    }
                }
                // 如果长度不同但已检查过前缀，还需检查新增/删除的部分
                if (!changed && beforeSigs.size() != program.instructions.size())
                    changed = true;
            }

            ++iter;
        }

        std::cerr << "[Optimizer] " << iter << " 轮, 最终 " << program.instructions.size() << " 条IR\n";
    }

    // ================================================================
    //  不可达代码删除：无条件跳转/返回后的指令直到下一个标签均为死代码
    // ================================================================
    void Optimizer::removeUnreachableCode(TACProgram &program)
    {
        std::vector<TACInstruction> kept;
        kept.reserve(program.instructions.size());
        bool reachable = true;
        int removed = 0;

        for (auto &instr : program.instructions)
        {
            // 标签总是保留，并且标记为可达（新的基本块入口）
            if (instr.type == TACType::LABEL)
            {
                reachable = true;
                kept.push_back(std::move(instr));
                continue;
            }

            // 不可达的非标签指令 → 跳过
            if (!reachable)
            {
                ++removed;
                continue;
            }

            kept.push_back(std::move(instr));

            // 无条件跳转或返回后，后续代码不可达
            if (kept.back().type == TACType::GOTO ||
                kept.back().type == TACType::RETURN)
            {
                reachable = false;
            }
        }

        program.instructions = std::move(kept);

        if (removed > 0)
            std::cerr << "[Unreachable] 删除: " << removed << " 条\n";
    }

    // ================================================================
    //  临时→局部合并：消除 %t = compute; var = %t 冗余拷贝
    //  例如:  %t5 = a + b        →   x = a + b
    //         x = %t5             (删除)
    //         后续所有 %t5 的引用 → x
    // ================================================================
    void Optimizer::coalesceTemporaryCopies(TACProgram &program)
    {
        std::vector<TACInstruction> result;
        result.reserve(program.instructions.size());
        int merged = 0;
        const auto &instrs = program.instructions;

        if (std::getenv("DEBUG_COALESCE"))
        {
            std::cerr << "[Coalesce] ENTER, " << instrs.size() << " instrs\n";
            for (size_t k = 0; k < instrs.size(); ++k)
            {
                const auto &ins = instrs[k];
                auto opTypeStr = [](const TACOperand &o) -> std::string
                {
                    switch (o.type)
                    {
                    case TACOpType::TEMP:
                        return "T(" + o.name + ")";
                    case TACOpType::VAR:
                        return "V(" + o.name + ")";
                    case TACOpType::CONST_INT:
                        return "C(" + std::to_string(o.intValue) + ")";
                    default:
                        return "_";
                    }
                };
                std::cerr << "  [" << k << "] type=" << static_cast<int>(ins.type)
                          << " result=" << opTypeStr(ins.result)
                          << " lhs=" << opTypeStr(ins.lhs)
                          << " rhs=" << opTypeStr(ins.rhs) << "\n";
            }
        }

        /// 判断两个操作数是否指向同一存储（同名即可，因为 IRGenerator 统一用 var() 包装）
        auto sameStorage = [](const TACOperand &a, const TACOperand &b) -> bool
        {
            // TEMP 和 VAR 在 IR 中通过名字区分（temps: t0,t1,...; vars: x.0,y.1,...）
            bool aIsNamed = (a.type == TACOpType::TEMP || a.type == TACOpType::VAR);
            bool bIsNamed = (b.type == TACOpType::TEMP || b.type == TACOpType::VAR);
            if (!aIsNamed || !bIsNamed)
                return false;
            return a.name == b.name;
        };

        /// 判断是否是"产生临时值"的指令（result 为 TEMP，且是 ASSIGN/BINARY/UNARY）
        auto producesTemp = [](const TACInstruction &instr) -> bool
        {
            if (instr.result.type != TACOpType::TEMP)
                return false;
            return instr.type == TACType::ASSIGN ||
                   instr.type == TACType::BINARY ||
                   instr.type == TACType::UNARY;
        };

        /// 判断是否是"拷贝到局部变量"的指令（ASSIGN, result=VAR, lhs 匹配某个 temp）
        /// 注意：IRGenerator 对所有操作数引用都用 var() 包装，所以 lhs 的 type 可能是
        /// VAR（即使它实际引用的是一个 temp）。因此这里按 name 匹配，不严格要求 type==TEMP。
        auto copyToLocal = [](const TACInstruction &instr, const TACOperand &temp) -> bool
        {
            if (instr.type != TACType::ASSIGN)
                return false;
            if (instr.result.type != TACOpType::VAR)
                return false;
            // lhs 必须是 VAR 或 TEMP（IRGenerator 统一用 var() 包装引用）
            if (instr.lhs.type != TACOpType::TEMP && instr.lhs.type != TACOpType::VAR)
                return false;
            // 不能是 var = var（同 storage）的自拷贝
            if (instr.lhs.type == TACOpType::VAR && instr.result.name == instr.lhs.name)
                return false;
            return instr.lhs.name == temp.name;
        };

        /// 判断指令是否阻止向前替换（控制流/标签/重定义）
        auto blocksSubstitution = [&](const TACInstruction &instr,
                                      const TACOperand &replacement) -> bool
        {
            if (instr.type == TACType::LABEL ||
                instr.type == TACType::GOTO ||
                instr.type == TACType::IF_GOTO ||
                instr.type == TACType::RETURN)
                return true;
            // 目标变量被重新赋值 → 停止
            if ((instr.type == TACType::ASSIGN ||
                 instr.type == TACType::BINARY ||
                 instr.type == TACType::UNARY ||
                 instr.type == TACType::FUNC_ARG) &&
                instr.result.type == TACOpType::VAR &&
                instr.result.name == replacement.name)
                return true;
            return false;
        };

        /// 替换指令中对 from 的引用为 to（按名字匹配，兼容 IRGenerator 的 var() 包装）
        auto replaceUse = [](TACInstruction &instr,
                             const TACOperand &from, const TACOperand &to)
        {
            auto matches = [&](const TACOperand &op) -> bool
            {
                // from 是 TEMP（来自 producesTemp），但引用处可能是 VAR
                bool opIsNamed = (op.type == TACOpType::TEMP || op.type == TACOpType::VAR);
                bool fromIsNamed = (from.type == TACOpType::TEMP || from.type == TACOpType::VAR);
                return opIsNamed && fromIsNamed && op.name == from.name;
            };
            if (matches(instr.lhs))
                instr.lhs = to;
            if (matches(instr.rhs))
                instr.rhs = to;
        };

        for (size_t i = 0; i < instrs.size(); ++i)
        {
            auto instr = instrs[i];

            // 检查模式: 指令 i 产生临时值，指令 i+1 将其拷贝到局部变量
            if (i + 1 < instrs.size() && producesTemp(instr))
            {
                const TACOperand temp = instr.result;
                if (copyToLocal(instrs[i + 1], temp))
                {
                    const TACOperand local = instrs[i + 1].result;

                    // 安全检查：确认 temp 在 i+1 之后是否被引用。
                    // 如果 temp 在控制流指令（LABEL/GOTO/IF_GOTO/RETURN）之后
                    // 仍被使用，则不能安全地合并（forward substitution 无法跨越控制流）。
                    bool safeToCoalesce = true;
                    bool hitControlFlow = false;
                    for (size_t j = i + 2; j < instrs.size(); ++j)
                    {
                        const auto& later = instrs[j];
                        // temp 被重定义 → 后续使用的是新值，安全
                        if ((later.type == TACType::ASSIGN || later.type == TACType::BINARY ||
                             later.type == TACType::UNARY || later.type == TACType::FUNC_ARG) &&
                            later.result.type != TACOpType::NONE &&
                            (later.result.type == TACOpType::TEMP || later.result.type == TACOpType::VAR) &&
                            later.result.name == temp.name)
                            break;  // temp 被重定义，后续使用安全

                        // 控制流指令
                        if (later.type == TACType::LABEL || later.type == TACType::GOTO ||
                            later.type == TACType::IF_GOTO || later.type == TACType::RETURN)
                            hitControlFlow = true;

                        // temp 在控制流之后被使用 → 不安全
                        if (hitControlFlow)
                        {
                            auto checkUse = [&](const TACOperand& op) {
                                if (op.type == TACOpType::TEMP || op.type == TACOpType::VAR)
                                    return op.name == temp.name;
                                return false;
                            };
                            if (checkUse(later.lhs) || checkUse(later.rhs))
                            {
                                safeToCoalesce = false;
                                break;
                            }
                        }
                    }

                    if (!safeToCoalesce)
                    {
                        // 不安全，保留原指令不变
                        result.push_back(std::move(instr));
                        continue;
                    }

                    // 将指令 i 的结果改为局部变量
                    instr.result = local;
                    result.push_back(std::move(instr));
                    ++i; // 跳过 i+1 (拷贝指令)
                    ++merged;

                    // 向前替换后续指令中对 temp 的引用
                    for (size_t j = i + 1; j < instrs.size(); ++j)
                    {
                        auto &later = const_cast<TACInstruction &>(instrs[j]);

                        // 遇到阻断条件 → 停止替换
                        if (blocksSubstitution(later, local))
                            break;

                        replaceUse(later, temp, local);
                    }
                    continue;
                }
            }

            result.push_back(std::move(instr));
        }

        program.instructions = std::move(result);

        if (merged > 0)
            std::cerr << "[Coalesce] 合并: " << merged << " 条\n";
        if (std::getenv("DEBUG_COALESCE"))
        {
            std::cerr << "[Coalesce] EXIT, merged=" << merged << ", " << program.instructions.size() << " instrs\n";
        }
    }

} // namespace MyCompiler
