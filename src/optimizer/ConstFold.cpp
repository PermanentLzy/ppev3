/// @file ConstFold.cpp
/// @brief 常量折叠 & 常量传播优化 Pass
///
/// 两阶段处理（在一次调用内迭代直到不动点）：
///   1. 常量传播: 追踪哪些临时变量/变量持有已知常量值，
///      将 ASSIGN x = t (t 为常量) → x = 常量
///   2. 常量折叠: 对 BINARY/UNARY 指令，若操作数映射到常量则编译期求值
///
/// 例如:
///   t0 = 3        → 记录 t0→3
///   t1 = 5        → 记录 t1→5
///   t2 = t0 + t1  → t2 = 3 + 5 → t2 = 8, 记录 t2→8
///   x = t2        → x = 8       (常量传播)
///   y = x + 1     → y = 8 + 1  → y = 9  (常量传播 + 折叠)

#include "Optimizer.h"
#include <iostream>
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace MyCompiler
{

    /// 对两个常量整数执行二元运算，返回结果
    static int foldBinary(int lhs, int rhs, const std::string &op)
    {
        if (op == "+")
            return lhs + rhs;
        if (op == "-")
            return lhs - rhs;
        if (op == "*")
            return lhs * rhs;
        if (op == "%")
        {
            if (rhs == 0)
                throw std::runtime_error("常量折叠: 模零错误");
            return lhs % rhs;
        }
        if (op == "/")
        {
            if (rhs == 0)
                throw std::runtime_error("常量折叠: 除零错误");
            return lhs / rhs;
        }
        if (op == "==")
            return (lhs == rhs) ? 1 : 0;
        if (op == "!=")
            return (lhs != rhs) ? 1 : 0;
        if (op == "<")
            return (lhs < rhs) ? 1 : 0;
        if (op == "<=")
            return (lhs <= rhs) ? 1 : 0;
        if (op == ">")
            return (lhs > rhs) ? 1 : 0;
        if (op == ">=")
            return (lhs >= rhs) ? 1 : 0;
        if (op == "&&")
            return (lhs && rhs) ? 1 : 0;
        if (op == "||")
            return (lhs || rhs) ? 1 : 0;

        throw std::runtime_error("常量折叠: 未知运算符 " + op);
    }

    /// 对常量执行一元运算
    static int foldUnary(int operand, const std::string &op)
    {
        if (op == "-")
            return -operand;
        if (op == "!")
            return (operand == 0) ? 1 : 0;
        throw std::runtime_error("常量折叠: 未知一元运算符 " + op);
    }

    /// 获取操作数对应的常量值（若已知），否则返回 false
    static bool tryGetConst(const TACOperand &op,
                            const std::unordered_map<std::string, int> &constMap,
                            int &outValue)
    {
        if (op.type == TACOpType::CONST_INT)
        {
            outValue = op.intValue;
            return true;
        }
        if ((op.type == TACOpType::VAR || op.type == TACOpType::TEMP) &&
            !op.name.empty())
        {
            auto it = constMap.find(op.name);
            if (it != constMap.end())
            {
                outValue = it->second;
                return true;
            }
        }
        return false;
    }

    /// 获取操作数名称（VAR/TEMP → name，CONST_INT → ""）
    static std::string opName(const TACOperand &op)
    {
        if (op.type == TACOpType::VAR || op.type == TACOpType::TEMP)
            return op.name;
        return "";
    }

    void Optimizer::constantFolding(TACProgram &program)
    {
        int totalFolded = 0;
        bool changed = true;

        while (changed)
        {
            changed = false;

            // ---- 第 1 遍：构建常量映射表 + 复写传播链 ----
            std::unordered_map<std::string, int> constMap;
            std::unordered_set<std::string> invalidated;

            for (auto &instr : program.instructions)
            {
                // ASSIGN result = CONST_INT → 记录
                if (instr.type == TACType::ASSIGN &&
                    instr.lhs.type == TACOpType::CONST_INT)
                {
                    std::string name = opName(instr.result);
                    if (!name.empty())
                        constMap[name] = instr.lhs.intValue;
                }
                // ASSIGN result = src (复写) → 传播
                else if (instr.type == TACType::ASSIGN &&
                         (instr.lhs.type == TACOpType::VAR || instr.lhs.type == TACOpType::TEMP))
                {
                    std::string srcName = instr.lhs.name;
                    auto it = constMap.find(srcName);
                    if (it != constMap.end())
                    {
                        std::string dstName = opName(instr.result);
                        if (!dstName.empty())
                            constMap[dstName] = it->second;
                    }
                }
                // 非常量写入 → 失效
                if (instr.type == TACType::ASSIGN &&
                    instr.lhs.type != TACOpType::CONST_INT)
                {
                    std::string name = opName(instr.result);
                    if (!name.empty())
                        invalidated.insert(name);
                }
                if (instr.type == TACType::BINARY || instr.type == TACType::UNARY)
                {
                    std::string name = opName(instr.result);
                    if (!name.empty())
                        invalidated.insert(name);
                }
            }
            for (auto &name : invalidated)
                constMap.erase(name);

            // ---- 第 2 遍：实际折叠/传播/化简 ----
            for (auto &instr : program.instructions)
            {
                // --- 常量传播: y = t (t→const) → y = const ---
                if (instr.type == TACType::ASSIGN &&
                    instr.lhs.type != TACOpType::CONST_INT)
                {
                    std::string srcName = opName(instr.lhs);
                    if (!srcName.empty())
                    {
                        auto it = constMap.find(srcName);
                        if (it != constMap.end())
                        {
                            instr.lhs = TACOperand::constInt(it->second);
                            changed = true;
                            ++totalFolded;
                        }
                    }
                }

                // --- BINARY 常量折叠 + 代数化简 ---
                if (instr.type == TACType::BINARY)
                {
                    int lv, rv;
                    bool lc = tryGetConst(instr.lhs, constMap, lv);
                    bool rc = tryGetConst(instr.rhs, constMap, rv);
                    bool same = (!instr.lhs.name.empty() && instr.lhs.name == instr.rhs.name);

                    // --- 常量传播: 将已知常量的 VAR/TEMP 操作数替换为 CONST_INT
                    // 这样 CodeGen 可用立即数形式指令（addi/slti/...），避免从栈上加载常量
                    if (lc && instr.lhs.type != TACOpType::CONST_INT)
                    {
                        instr.lhs = TACOperand::constInt(lv);
                        changed = true;
                        ++totalFolded;
                    }
                    if (rc && instr.rhs.type != TACOpType::CONST_INT)
                    {
                        instr.rhs = TACOperand::constInt(rv);
                        changed = true;
                        ++totalFolded;
                    }
                    // 重新读取（instr.lhs/rhs 可能已被替换为 CONST_INT）
                    lc = tryGetConst(instr.lhs, constMap, lv);
                    rc = tryGetConst(instr.rhs, constMap, rv);

                    if (lc && rc)
                    {
                        try
                        {
                            int result = foldBinary(lv, rv, instr.op);
                            instr.type = TACType::ASSIGN;
                            instr.lhs = TACOperand::constInt(result);
                            instr.rhs = TACOperand::none();
                            instr.op.clear();
                            changed = true;
                            ++totalFolded;
                            continue;
                        }
                        catch (...)
                        {
                        }
                    }
                    // 同变量: x==x→1, x!=x→0, x<x→0, x<=x→1, x-x→0
                    if (same)
                    {
                        if (instr.op == "==" || instr.op == "<=" || instr.op == ">=")
                        {
                            instr.type = TACType::ASSIGN;
                            instr.lhs = TACOperand::constInt(1);
                            instr.rhs = TACOperand::none();
                            instr.op.clear();
                            changed = true;
                            ++totalFolded;
                            continue;
                        }
                        if (instr.op == "!=" || instr.op == "<" || instr.op == ">")
                        {
                            instr.type = TACType::ASSIGN;
                            instr.lhs = TACOperand::constInt(0);
                            instr.rhs = TACOperand::none();
                            instr.op.clear();
                            changed = true;
                            ++totalFolded;
                            continue;
                        }
                        if (instr.op == "-")
                        {
                            instr.type = TACType::ASSIGN;
                            instr.lhs = TACOperand::constInt(0);
                            instr.rhs = TACOperand::none();
                            instr.op.clear();
                            changed = true;
                            ++totalFolded;
                            continue;
                        }
                    }
                    // 代数化简: x+0→x, x-0→x
                    if (rc && rv == 0 && (instr.op == "+" || instr.op == "-"))
                    {
                        instr.type = TACType::ASSIGN;
                        instr.rhs = TACOperand::none();
                        instr.op.clear();
                        changed = true;
                        ++totalFolded;
                        continue;
                    }
                    // 0+x→x
                    if (lc && lv == 0 && instr.op == "+")
                    {
                        instr.type = TACType::ASSIGN;
                        instr.lhs = instr.rhs;
                        instr.rhs = TACOperand::none();
                        instr.op.clear();
                        changed = true;
                        ++totalFolded;
                        continue;
                    }
                    // x*1→x
                    if (rc && rv == 1 && instr.op == "*")
                    {
                        instr.type = TACType::ASSIGN;
                        instr.rhs = TACOperand::none();
                        instr.op.clear();
                        changed = true;
                        ++totalFolded;
                        continue;
                    }
                    // 1*x→x
                    if (lc && lv == 1 && instr.op == "*")
                    {
                        instr.type = TACType::ASSIGN;
                        instr.lhs = instr.rhs;
                        instr.rhs = TACOperand::none();
                        instr.op.clear();
                        changed = true;
                        ++totalFolded;
                        continue;
                    }
                    // x*0→0
                    if ((lc && lv == 0) || (rc && rv == 0))
                    {
                        if (instr.op == "*")
                        {
                            instr.type = TACType::ASSIGN;
                            instr.lhs = TACOperand::constInt(0);
                            instr.rhs = TACOperand::none();
                            instr.op.clear();
                            changed = true;
                            ++totalFolded;
                            continue;
                        }
                    }
                    // x/1→x
                    if (rc && rv == 1 && instr.op == "/")
                    {
                        instr.type = TACType::ASSIGN;
                        instr.rhs = TACOperand::none();
                        instr.op.clear();
                        changed = true;
                        ++totalFolded;
                        continue;
                    }
                    // x%1→0
                    if (rc && rv == 1 && instr.op == "%")
                    {
                        instr.type = TACType::ASSIGN;
                        instr.lhs = TACOperand::constInt(0);
                        instr.rhs = TACOperand::none();
                        instr.op.clear();
                        changed = true;
                        ++totalFolded;
                        continue;
                    }
                    // 0/x→0  (x≠0, 因为 lc&&rc 已处理)
                    if (lc && lv == 0 && (instr.op == "/" || instr.op == "%"))
                    {
                        instr.type = TACType::ASSIGN;
                        instr.lhs = TACOperand::constInt(0);
                        instr.rhs = TACOperand::none();
                        instr.op.clear();
                        changed = true;
                        ++totalFolded;
                        continue;
                    }
                }

                // --- IF_GOTO 常量条件化简: if (1) goto L → goto L; if (0) goto L → 删除 ---
                if (instr.type == TACType::IF_GOTO)
                {
                    int cv;
                    if (tryGetConst(instr.lhs, constMap, cv))
                    {
                        if (cv != 0)
                        {
                            // 条件恒真 → 无条件跳转
                            instr.type = TACType::GOTO;
                            instr.lhs = TACOperand::none();
                        }
                        else
                        {
                            // 条件恒假 → 删除（改为 NOP，后续 DCE 会清除）
                            instr.type = TACType::NOP;
                            instr.lhs = TACOperand::none();
                            instr.label.clear();
                        }
                        changed = true;
                        ++totalFolded;
                        continue;
                    }
                }

                // --- UNARY 常量折叠 ---
                if (instr.type == TACType::UNARY)
                {
                    int ov;
                    // 常量传播: 将已知常量的操作数替换为 CONST_INT
                    if (tryGetConst(instr.lhs, constMap, ov) && instr.lhs.type != TACOpType::CONST_INT)
                    {
                        instr.lhs = TACOperand::constInt(ov);
                        changed = true;
                        ++totalFolded;
                    }
                    if (tryGetConst(instr.lhs, constMap, ov))
                    {
                        try
                        {
                            int result = foldUnary(ov, instr.op);
                            instr.type = TACType::ASSIGN;
                            instr.lhs = TACOperand::constInt(result);
                            instr.op.clear();
                            changed = true;
                            ++totalFolded;
                        }
                        catch (...)
                        {
                        }
                    }
                }
            }
        }

        if (totalFolded > 0)
            std::cerr << "[ConstFold] 折叠/化简/传播: " << totalFolded << " 条\n";
    }

} // namespace MyCompiler
