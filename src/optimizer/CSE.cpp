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

namespace MyCompiler
{

    /// 为 BINARY 指令生成唯一键: "op|lhsStr|rhsStr"
    static std::string makeBinaryKey(const TACInstruction &instr)
    {
        return instr.op + "|" + instr.lhs.toString() + "|" + instr.rhs.toString();
    }

    /// 为 UNARY 指令生成唯一键: "op|lhsStr"
    static std::string makeUnaryKey(const TACInstruction &instr)
    {
        return instr.op + "|" + instr.lhs.toString();
    }

    /// 从缓存中删除所有包含指定变量名作为完整 token 的条目
    /// cache key 格式:
    ///   表达式缓存: "op|lhsStr|rhsStr" (BINARY) 或 "op|lhsStr" (UNARY)
    ///   复制缓存:   "CP_varName" (ASSIGN 源到目标的映射)
    /// 必须按 | 分割后逐 token 精确匹配，避免子串误匹配（如 "a" 匹配到 "ab"）
    static void invalidateVar(std::unordered_map<std::string, std::string> &cache,
                              const std::string &varName)
    {
        std::vector<std::string> toRemove;
        for (auto &kv : cache)
        {
            const std::string &key = kv.first;
            bool found = false;

            // 检查 CP_ 前缀的复制缓存
            if (key.size() > 3 && key.substr(0, 3) == "CP_")
            {
                if (key.substr(3) == varName)
                {
                    found = true;
                }
            }

            // 检查表达式缓存: 按 | 分割 key，检查是否有 token 完全等于 varName
            if (!found)
            {
                size_t start = 0;
                size_t pos;
                while ((pos = key.find('|', start)) != std::string::npos)
                {
                    if (key.substr(start, pos - start) == varName)
                    {
                        found = true;
                        break;
                    }
                    start = pos + 1;
                }
                // 检查最后一个 token（rhsStr 部分，无尾部 |）
                if (!found && key.substr(start) == varName)
                {
                    found = true;
                }
            }

            if (found)
            {
                toRemove.push_back(key);
            }
        }
        for (auto &key : toRemove)
        {
            cache.erase(key);
        }
    }

    void Optimizer::commonSubexpressionElimination(TACProgram &program)
    {
        std::unordered_map<std::string, std::string> cache;
        // 收集被跳转引用的标签（只有这些清缓存，避免过度失效）
        std::unordered_set<std::string> jumpTargets;
        for (auto &instr : program.instructions)
        {
            if ((instr.type == TACType::GOTO || instr.type == TACType::IF_GOTO) && !instr.label.empty())
                jumpTargets.insert(instr.label);
        }

        int eliminated = 0;
        for (auto &instr : program.instructions)
        {
            // --- 只在跳转目标处重置缓存 ---
            if (instr.type == TACType::LABEL && jumpTargets.count(instr.label))
            {
                cache.clear();
                continue;
            }

            // --- 变量赋值：使涉及该变量的缓存失效 ---
            if (instr.type == TACType::ASSIGN &&
                (instr.result.type == TACOpType::VAR || instr.result.type == TACOpType::TEMP))
            {
                invalidateVar(cache, instr.result.name);
            }
            if ((instr.type == TACType::BINARY || instr.type == TACType::UNARY ||
                 instr.type == TACType::FUNC_ARG) &&
                !instr.result.name.empty())
            {
                invalidateVar(cache, instr.result.name);
            }

            // --- BINARY CSE ---
            if (instr.type == TACType::BINARY &&
                instr.lhs.type != TACOpType::CONST_INT && instr.rhs.type != TACOpType::CONST_INT)
            {
                std::string key = makeBinaryKey(instr);
                auto it = cache.find(key);
                if (it != cache.end())
                {
                    instr.type = TACType::ASSIGN;
                    instr.lhs = TACOperand::var(it->second);
                    instr.rhs = TACOperand::none();
                    instr.op.clear();
                    ++eliminated;
                }
                else if (!instr.result.name.empty())
                {
                    cache[key] = instr.result.name;
                }
            }

            // --- UNARY CSE ---
            if (instr.type == TACType::UNARY &&
                instr.lhs.type != TACOpType::CONST_INT)
            {
                std::string key = makeUnaryKey(instr);
                auto it = cache.find(key);
                if (it != cache.end())
                {
                    instr.type = TACType::ASSIGN;
                    instr.lhs = TACOperand::var(it->second);
                    instr.op.clear();
                    ++eliminated;
                }
                else if (!instr.result.name.empty())
                {
                    cache[key] = instr.result.name;
                }
            }

            // --- ASSIGN 传播：x = y 后，后续用 y 替换为 x 的缓存 ---
            if (instr.type == TACType::ASSIGN &&
                instr.lhs.type != TACOpType::CONST_INT &&
                (instr.result.type == TACOpType::VAR || instr.result.type == TACOpType::TEMP))
            {
                std::string srcKey = "CP_" + instr.lhs.name;
                cache[srcKey] = instr.result.name;
            }
        }

        if (eliminated > 0)
            std::cerr << "[CSE] 消除: " << eliminated << " 条\n";
    }

} // namespace MyCompiler
