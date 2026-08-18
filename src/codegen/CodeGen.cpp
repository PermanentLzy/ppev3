#include "CodeGen.h"
#include <iostream>
#include <algorithm>
#include <sstream>

namespace MyCompiler
{

    // 可用的 s 寄存器池（s0-s11，共 12 个 callee-saved 寄存器）
    const std::vector<std::string> CodeGen::sRegPool_ = {
        "s0", "s1", "s2", "s3", "s4", "s5",
        "s6", "s7", "s8", "s9", "s10", "s11"};

    void CodeGen::generate(const TACProgram &program)
    {
        // ============================================================
        //  第 0 遍：收集信息
        // ============================================================
        labelMap_.clear();
        varOffsets_.clear();
        funcNames_.clear();
        paramQueue_.clear();
        currentFunc_.clear();
        funcArgIndex_ = 0;
        funcReturned_ = false;
        currentFrameSize_ = 0;
        lastLineValid_ = false;
        lastEmittedLine_.clear();
        lastStoreValid_ = false;
        lastEmittedWasRet_ = false;
        int labelCounter = 0;

        // 收集所有标签 → RISC-V 标签映射，同时识别函数入口
        for (auto &instr : program.instructions)
        {
            if (instr.type == TACType::LABEL)
            {
                if (labelMap_.find(instr.label) == labelMap_.end())
                {
                    labelMap_[instr.label] = ".L" + std::to_string(labelCounter++);
                }
                if (instr.label.size() > 5 && instr.label.substr(0, 5) == "func_")
                {
                    funcNames_.insert(instr.label.substr(5));
                }
            }
            if (instr.type == TACType::CALL && !instr.label.empty())
            {
                funcNames_.insert(instr.label);
            }
        }

        // ============================================================
        //  第 1 遍：收集全局变量初始化（迭代常量折叠）
        // ============================================================
        globalVars_.clear();
        globalInit_.clear();
        bool pastFirstFunc = false;
        std::unordered_map<std::string, int> constMap; // 变量名→常量值

        // 辅助：尝试获取操作数的常量值
        auto tryGet = [&](const TACOperand &op, int &val) -> bool
        {
            if (op.type == TACOpType::CONST_INT)
            {
                val = op.intValue;
                return true;
            }
            auto it = constMap.find(op.name);
            if (it != constMap.end())
            {
                val = it->second;
                return true;
            }
            return false;
        };
        // 辅助：折叠二元运算
        auto foldBin = [](int l, int r, const std::string &op) -> int
        {
            if (op == "+")
                return l + r;
            if (op == "-")
                return l - r;
            if (op == "*")
                return l * r;
            if (op == "/")
                return r ? l / r : 0;
            if (op == "%")
                return r ? l % r : 0;
            return 0;
        };

        // 迭代直到不动点（处理常量传播链）
        bool changed = true;
        while (changed)
        {
            changed = false;
            pastFirstFunc = false;
            for (auto &instr : program.instructions)
            {
                if (instr.type == TACType::LABEL &&
                    instr.label.size() > 5 && instr.label.substr(0, 5) == "func_")
                    pastFirstFunc = true;
                if (pastFirstFunc)
                    continue;

                // ASSIGN: 将已知常量传播到目标
                if (instr.type == TACType::ASSIGN)
                {
                    int val;
                    if (tryGet(instr.lhs, val))
                    {
                        auto &name = instr.result.name;
                        if (!name.empty() && constMap.find(name) == constMap.end())
                        {
                            constMap[name] = val;
                            if (instr.result.type == TACOpType::VAR)
                            {
                                globalVars_.insert(name);
                                globalInit_.push_back({name, val});
                            }
                            changed = true;
                        }
                    }
                }
                // BINARY: 编译期求值
                if (instr.type == TACType::BINARY)
                {
                    int lv, rv;
                    if (tryGet(instr.lhs, lv) && tryGet(instr.rhs, rv))
                    {
                        int val = foldBin(lv, rv, instr.op);
                        auto &name = instr.result.name;
                        if (!name.empty() && constMap.find(name) == constMap.end())
                        {
                            constMap[name] = val;
                            if (instr.result.type == TACOpType::VAR)
                            {
                                globalVars_.insert(name);
                                globalInit_.push_back({name, val});
                            }
                            changed = true;
                        }
                    }
                }
                // UNARY: 编译期求值
                if (instr.type == TACType::UNARY)
                {
                    int ov;
                    if (tryGet(instr.lhs, ov))
                    {
                        int val = (instr.op == "-") ? -ov : (ov == 0 ? 1 : 0);
                        auto &name = instr.result.name;
                        if (!name.empty() && constMap.find(name) == constMap.end())
                        {
                            constMap[name] = val;
                            if (instr.result.type == TACOpType::VAR)
                            {
                                globalVars_.insert(name);
                                globalInit_.push_back({name, val});
                            }
                            changed = true;
                        }
                    }
                }
            }
        }

        // ============================================================
        //  输出 .data 段（全局变量）
        // ============================================================
        if (!globalInit_.empty())
        {
            emit(".data");
            for (auto &gv : globalInit_)
            {
                emit("_g_" + gv.first + ": .word " + std::to_string(gv.second));
            }
        }

        // ============================================================
        //  预扫描：统计每个函数的变量数，计算动态帧大小
        // ============================================================
        std::unordered_map<std::string, int> funcFrameSizes;
        {
            std::string curFn;
            std::unordered_set<std::string> varNames;
            for (auto &instr : program.instructions)
            {
                if (instr.type == TACType::LABEL && instr.label.find("func_") == 0)
                {
                    if (!curFn.empty())
                    {
                        int frame = std::max(256, (static_cast<int>(varNames.size()) + 4) * 4);
                        funcFrameSizes[curFn] = frame;
                    }
                    curFn = instr.label.substr(5);
                    varNames.clear();
                }
                // 收集所有变量/临时变量名
                auto collect = [&](const TACOperand &op)
                {
                    if ((op.type == TACOpType::VAR || op.type == TACOpType::TEMP) && !op.name.empty())
                        varNames.insert(op.name);
                };
                collect(instr.result);
                collect(instr.lhs);
                collect(instr.rhs);
            }
            if (!curFn.empty())
            {
                int frame = std::max(256, (static_cast<int>(varNames.size()) + 4) * 4);
                funcFrameSizes[curFn] = frame;
            }
        }

        // 计算最大帧大小（用于跨函数参数传递缓冲）
        int maxFrameSize = 256;
        for (auto &kv : funcFrameSizes)
            if (kv.second > maxFrameSize)
                maxFrameSize = kv.second;

        // ============================================================
        //  寄存器分配预扫描：为每个函数选择使用最频繁的局部变量
        //  分配到 s0-s11 寄存器，避免循环/热点中的栈访问
        //  注意：仅对包含循环（后向跳转）的函数分配，避免在简单函数中
        //  增加 save/restore 开销
        // ============================================================
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> funcRegAlloc;
        {
            std::string curFn;
            // 检测每个函数是否有循环（后向 GOTO）
            std::unordered_set<std::string> funcHasLoop;
            {
                std::vector<std::string> labelStack;
                std::unordered_map<std::string, size_t> labelPos;
                for (size_t i = 0; i < program.instructions.size(); ++i)
                {
                    auto &instr = program.instructions[i];
                    if (instr.type == TACType::LABEL &&
                        instr.label.size() > 5 && instr.label.substr(0, 5) == "func_")
                    {
                        curFn = instr.label.substr(5);
                        labelPos.clear();
                        continue;
                    }
                    if (instr.type == TACType::LABEL)
                        labelPos[instr.label] = i;
                    if (instr.type == TACType::GOTO && !curFn.empty())
                    {
                        // 后向跳转 = 循环
                        auto it = labelPos.find(instr.label);
                        if (it != labelPos.end() && it->second < i)
                            funcHasLoop.insert(curFn);
                    }
                }
            }

            curFn.clear();
            // 统计每个函数中每个命名操作数（VAR 或 TEMP）的使用次数
            std::unordered_map<std::string, std::unordered_map<std::string, int>> funcVarUsage;
            for (auto &instr : program.instructions)
            {
                if (instr.type == TACType::LABEL &&
                    instr.label.size() > 5 && instr.label.substr(0, 5) == "func_")
                {
                    curFn = instr.label.substr(5);
                    continue;
                }
                if (curFn.empty())
                    continue;

                auto countVar = [&](const TACOperand &op)
                {
                    if ((op.type == TACOpType::VAR || op.type == TACOpType::TEMP) && !op.name.empty())
                        funcVarUsage[curFn][op.name]++;
                };
                countVar(instr.result);
                countVar(instr.lhs);
                countVar(instr.rhs);
            }

            // 为每个函数分配 s 寄存器（按使用次数降序，最多 12 个）
            for (auto &kv : funcVarUsage)
            {
                const std::string &fn = kv.first;
                // 只对包含循环的函数分配 s 寄存器
                if (!funcHasLoop.count(fn))
                    continue;

                auto &usage = kv.second;
                std::vector<std::pair<std::string, int>> sorted(usage.begin(), usage.end());
                std::sort(sorted.begin(), sorted.end(),
                          [](const auto &a, const auto &b)
                          { return a.second > b.second; });

                auto &regAlloc = funcRegAlloc[fn];
                for (size_t i = 0; i < sorted.size() && i < sRegPool_.size(); ++i)
                {
                    // 只分配使用次数 >= 2 的变量
                    if (sorted[i].second < 2)
                        break;
                    regAlloc[sorted[i].first] = sRegPool_[i];
                }
            }
        }

        // ============================================================
        //  输出程序头部（_start 入口 + 栈初始化 + 调用 main）
        // ============================================================
        emitPrologue();

        // _start 调用 main，返回值在 a0 中，随后 exit syscall
        emit("call main");
        emitExit(); // exit syscall: li a7, 93; ecall

        // ============================================================
        //  遍历 TAC 指令，生成 RISC-V 汇编
        //  IR 不再有 GOTO wrapper，函数体直接顺序输出
        // ============================================================
        for (auto &instr : program.instructions)
        {
            // --- 函数入口标签 ---
            if (instr.type == TACType::LABEL)
            {
                std::string funcName;
                if (instr.label.size() > 5 && instr.label.substr(0, 5) == "func_")
                {
                    funcName = instr.label.substr(5);
                }

                if (!funcName.empty() && funcNames_.count(funcName))
                {
                    // 前一个函数的 fallback epilogue：若最后一条已是 ret 则跳过（死代码消除）
                    if (!currentFunc_.empty() && !lastEmittedWasRet_)
                        emitFuncEpilogue();

                    currentFunc_ = funcName;
                    varOffsets_.clear();
                    funcArgIndex_ = 0;
                    funcReturned_ = false;
                    currentFrameSize_ = 0;
                    // 重置 peephole 状态，避免跨函数误优化
                    lastStoreValid_ = false;
                    lastLineValid_ = false;
                    lastEmittedLine_.clear();
                    lastMvFromA0Valid_ = false;

                    // 设置当前函数的寄存器分配
                    varToReg_.clear();
                    usedSRegs_.clear();
                    if (funcRegAlloc.count(funcName))
                    {
                        varToReg_ = funcRegAlloc[funcName];
                        for (auto &kv : varToReg_)
                            usedSRegs_.push_back(kv.second);
                    }

                    int fs = funcFrameSizes.count(funcName) ? funcFrameSizes[funcName] : 256;
                    emitFuncPrologue(funcName, fs);
                    continue;
                }
            }

            // --- RETURN ---
            if (instr.type == TACType::RETURN)
            {
                if (!currentFunc_.empty())
                {
                    if (instr.lhs.type != TACOpType::NONE)
                    {
                        // Peephole: 如果上一条指令是 CALL 产生的 "mv sN, a0"
                        // 且 RETURN 使用同一个变量，a0 已有值，跳过 mv
                        if (lastMvFromA0Valid_ &&
                            (instr.lhs.type == TACOpType::VAR || instr.lhs.type == TACOpType::TEMP) &&
                            varToReg_.count(instr.lhs.name) &&
                            varToReg_[instr.lhs.name] == lastMvFromA0_)
                        {
                            // a0 already has the return value, skip mv
                        }
                        else
                        {
                            std::string retReg = loadOperand(instr.lhs, "a0");
                            if (retReg != "a0")
                                emit("mv a0, " + retReg);
                        }
                    }
                    else
                        emit("li a0, 0");
                    lastMvFromA0Valid_ = false;
                    emitFuncEpilogue();
                }
                continue;
            }

            // --- 指令翻译 ---
            switch (instr.type)
            {
            case TACType::PARAM:
            {
                paramQueue_.push_back(instr.lhs.name);
                break;
            }

            case TACType::FUNC_ARG:
            {
                if (funcArgIndex_ < 8)
                {
                    std::string argReg = "a" + std::to_string(funcArgIndex_);
                    // 如果参数被分配到 s 寄存器，直接 mv
                    if ((instr.result.type == TACOpType::VAR || instr.result.type == TACOpType::TEMP) && varToReg_.count(instr.result.name))
                    {
                        emit("mv " + varToReg_[instr.result.name] + ", " + argReg);
                    }
                    else
                    {
                        storeOperand(instr.result, argReg);
                    }
                }
                else
                {
                    int offset = currentFrameSize_ + maxFrameSize + (funcArgIndex_ - 8) * 4;
                    emitStackLoad("t0", offset);
                    storeOperand(instr.result, "t0");
                }
                funcArgIndex_++;
                break;
            }

            case TACType::ASSIGN:
            {
                // x = y
                // 优化：如果源是常数且目标在 s 寄存器中，直接 li sN, value
                if (instr.lhs.type == TACOpType::CONST_INT &&
                    (instr.result.type == TACOpType::VAR || instr.result.type == TACOpType::TEMP) &&
                    varToReg_.count(instr.result.name))
                {
                    emit("li " + varToReg_[instr.result.name] + ", " + std::to_string(instr.lhs.intValue));
                    break;
                }
                // 优化：如果源和目标都在 s 寄存器中且相同，无需指令
                if ((instr.lhs.type == TACOpType::VAR || instr.lhs.type == TACOpType::TEMP) &&
                    varToReg_.count(instr.lhs.name) &&
                    (instr.result.type == TACOpType::VAR || instr.result.type == TACOpType::TEMP) &&
                    varToReg_.count(instr.result.name))
                {
                    if (varToReg_[instr.lhs.name] != varToReg_[instr.result.name])
                        emit("mv " + varToReg_[instr.result.name] + ", " + varToReg_[instr.lhs.name]);
                    break;
                }
                // 通用情况
                std::string srcReg = loadOperand(instr.lhs, "t0");
                storeOperand(instr.result, srcReg);
                break;
            }

            case TACType::BINARY:
            {
                // x = y op z
                // 优化：右操作数为常数时使用 immediate 形式指令
                if (instr.rhs.type == TACOpType::CONST_INT)
                {
                    int c = instr.rhs.intValue;
                    // 加法：addi
                    if (instr.op == "+" && c >= -2048 && c <= 2047)
                    {
                        std::string lhsReg = loadOperand(instr.lhs, "t0");
                        // 如果 lhsReg 是 s 寄存器，结果也需写入 s 寄存器
                        std::string dstReg = ((instr.result.type == TACOpType::VAR || instr.result.type == TACOpType::TEMP) && varToReg_.count(instr.result.name))
                                                 ? varToReg_[instr.result.name]
                                                 : "t0";
                        emit("addi " + dstReg + ", " + lhsReg + ", " + std::to_string(c));
                        if (dstReg == "t0")
                            storeOperand(instr.result, "t0");
                        break;
                    }
                    // 减法：addi 负数
                    if (instr.op == "-" && c >= -2047 && c <= 2048)
                    {
                        std::string lhsReg = loadOperand(instr.lhs, "t0");
                        std::string dstReg = ((instr.result.type == TACOpType::VAR || instr.result.type == TACOpType::TEMP) && varToReg_.count(instr.result.name))
                                                 ? varToReg_[instr.result.name]
                                                 : "t0";
                        emit("addi " + dstReg + ", " + lhsReg + ", " + std::to_string(-c));
                        if (dstReg == "t0")
                            storeOperand(instr.result, "t0");
                        break;
                    }
                    // 乘以 2^n：slli
                    if (instr.op == "*" && c > 0 && (c & (c - 1)) == 0)
                    {
                        int shift = 0, t = c;
                        while (t > 1)
                        {
                            t >>= 1;
                            ++shift;
                        }
                        std::string lhsReg = loadOperand(instr.lhs, "t0");
                        std::string dstReg = ((instr.result.type == TACOpType::VAR || instr.result.type == TACOpType::TEMP) && varToReg_.count(instr.result.name))
                                                 ? varToReg_[instr.result.name]
                                                 : "t0";
                        emit("slli " + dstReg + ", " + lhsReg + ", " + std::to_string(shift));
                        if (dstReg == "t0")
                            storeOperand(instr.result, "t0");
                        break;
                    }
                    // 除以 2^n：srai（算术右移，含负数修正）
                    if (instr.op == "/" && c > 0 && (c & (c - 1)) == 0)
                    {
                        int shift = 0, t = c;
                        while (t > 1)
                        {
                            t >>= 1;
                            ++shift;
                        }
                        std::string lhsReg = loadOperand(instr.lhs, "t0");
                        // 需要使用 t0 作为工作寄存器（涉及多步计算）
                        if (lhsReg != "t0")
                            emit("mv t0, " + lhsReg);
                        emit("srai t1, t0, 31");
                        if (c - 1 <= 2047)
                            emit("andi t1, t1, " + std::to_string(c - 1));
                        else
                        {
                            emit("li t2, " + std::to_string(c - 1));
                            emit("and t1, t1, t2");
                        }
                        emit("add t0, t0, t1");
                        emit("srai t0, t0, " + std::to_string(shift));
                        storeOperand(instr.result, "t0");
                        break;
                    }
                    // 与 0 比较
                    if (c == 0 && (instr.op == "==" || instr.op == "!="))
                    {
                        std::string lhsReg = loadOperand(instr.lhs, "t0");
                        std::string dstReg = ((instr.result.type == TACOpType::VAR || instr.result.type == TACOpType::TEMP) && varToReg_.count(instr.result.name))
                                                 ? varToReg_[instr.result.name]
                                                 : "t0";
                        if (instr.op == "==")
                            emit("seqz " + dstReg + ", " + lhsReg);
                        else
                            emit("snez " + dstReg + ", " + lhsReg);
                        if (dstReg == "t0")
                            storeOperand(instr.result, "t0");
                        break;
                    }
                    // 比较运算符与常数 RHS
                    if (c >= -2048 && c <= 2047 &&
                        (instr.op == "<" || instr.op == ">="))
                    {
                        std::string lhsReg = loadOperand(instr.lhs, "t0");
                        std::string dstReg = ((instr.result.type == TACOpType::VAR || instr.result.type == TACOpType::TEMP) && varToReg_.count(instr.result.name))
                                                 ? varToReg_[instr.result.name]
                                                 : "t0";
                        emit("slti " + dstReg + ", " + lhsReg + ", " + std::to_string(c));
                        if (instr.op == ">=")
                            emit("xori " + dstReg + ", " + dstReg + ", 1");
                        if (dstReg == "t0")
                            storeOperand(instr.result, "t0");
                        break;
                    }
                }

                // 通用情况：加载左右操作数，使用返回的寄存器名
                {
                    std::string lhsReg = loadOperand(instr.lhs, "t0");
                    std::string rhsReg = loadOperand(instr.rhs, "t1");
                    // 确定结果寄存器：如果 result 在 s 寄存器中，直接写入
                    std::string dstReg = ((instr.result.type == TACOpType::VAR || instr.result.type == TACOpType::TEMP) && varToReg_.count(instr.result.name))
                                             ? varToReg_[instr.result.name]
                                             : "t0";

                    // 如果 dstReg 与 lhsReg 或 rhsReg 相同，且需要多步计算，
                    // 可能会覆盖源操作数。对于单指令运算（add/sub/mul等），
                    // RISC-V 三操作数格式保证先读后写，安全。
                    // 但对于多步运算（<=, >=, ==, !=），需要用 t0 作为中间寄存器。
                    bool multiStep = (instr.op == "<=" || instr.op == ">=" ||
                                      instr.op == "==" || instr.op == "!=");
                    if (multiStep)
                        dstReg = "t0";

                    if (instr.op == "+")
                        emit("add " + dstReg + ", " + lhsReg + ", " + rhsReg);
                    else if (instr.op == "-")
                        emit("sub " + dstReg + ", " + lhsReg + ", " + rhsReg);
                    else if (instr.op == "*")
                        emit("mul " + dstReg + ", " + lhsReg + ", " + rhsReg);
                    else if (instr.op == "/")
                        emit("div " + dstReg + ", " + lhsReg + ", " + rhsReg);
                    else if (instr.op == "%")
                        emit("rem " + dstReg + ", " + lhsReg + ", " + rhsReg);
                    else if (instr.op == "<")
                        emit("slt " + dstReg + ", " + lhsReg + ", " + rhsReg);
                    else if (instr.op == "<=")
                    {
                        emit("slt t0, " + rhsReg + ", " + lhsReg);
                        emit("seqz " + dstReg + ", t0");
                    }
                    else if (instr.op == ">")
                        emit("slt " + dstReg + ", " + rhsReg + ", " + lhsReg);
                    else if (instr.op == ">=")
                    {
                        emit("slt t0, " + lhsReg + ", " + rhsReg);
                        emit("seqz " + dstReg + ", t0");
                    }
                    else if (instr.op == "==")
                    {
                        emit("sub t0, " + lhsReg + ", " + rhsReg);
                        emit("seqz " + dstReg + ", t0");
                    }
                    else if (instr.op == "!=")
                    {
                        emit("sub t0, " + lhsReg + ", " + rhsReg);
                        emit("snez " + dstReg + ", t0");
                    }
                    else if (instr.op == "&&")
                        emit("and " + dstReg + ", " + lhsReg + ", " + rhsReg);
                    else if (instr.op == "||")
                        emit("or " + dstReg + ", " + lhsReg + ", " + rhsReg);
                    else if (instr.op == "<<")
                    {
                        if (instr.rhs.type == TACOpType::CONST_INT &&
                            instr.rhs.intValue >= 0 && instr.rhs.intValue <= 31)
                            emit("slli " + dstReg + ", " + lhsReg + ", " + std::to_string(instr.rhs.intValue));
                        else
                        {
                            emit("sll " + dstReg + ", " + lhsReg + ", " + rhsReg);
                        }
                    }
                    else if (instr.op == ">>")
                    {
                        if (instr.rhs.type == TACOpType::CONST_INT &&
                            instr.rhs.intValue >= 0 && instr.rhs.intValue <= 31)
                            emit("srai " + dstReg + ", " + lhsReg + ", " + std::to_string(instr.rhs.intValue));
                        else
                        {
                            emit("sra " + dstReg + ", " + lhsReg + ", " + rhsReg);
                        }
                    }

                    if (dstReg == "t0")
                        storeOperand(instr.result, "t0");
                }
                break;
            }

            case TACType::UNARY:
            {
                // x = op y
                std::string lhsReg = loadOperand(instr.lhs, "t0");
                std::string dstReg = ((instr.result.type == TACOpType::VAR || instr.result.type == TACOpType::TEMP) && varToReg_.count(instr.result.name))
                                         ? varToReg_[instr.result.name]
                                         : "t0";

                if (instr.op == "-")
                    emit("neg " + dstReg + ", " + lhsReg);
                else if (instr.op == "!")
                    emit("seqz " + dstReg + ", " + lhsReg);

                if (dstReg == "t0")
                    storeOperand(instr.result, "t0");
                break;
            }

            case TACType::GOTO:
            {
                std::string rvLabel = labelMap_.count(instr.label) ? labelMap_[instr.label] : instr.label;
                emit("j " + rvLabel);
                break;
            }

            case TACType::IF_GOTO:
            {
                std::string condReg = loadOperand(instr.lhs, "t0");
                std::string rvLabel = labelMap_.count(instr.label) ? labelMap_[instr.label] : instr.label;
                emit("bnez " + condReg + ", " + rvLabel);
                break;
            }

            case TACType::LABEL:
            {
                std::string rvLabel = labelMap_.count(instr.label) ? labelMap_[instr.label] : instr.label;
                emit(rvLabel + ":");
                break;
            }

            case TACType::CALL:
            {
                // 将参数队列中的值加载到 a0-a7，额外参数放栈上
                int argCount = std::min(static_cast<int>(paramQueue_.size()), static_cast<int>(instr.lhs.intValue));
                int regArgs = std::min(argCount, 8);
                for (int i = 0; i < regArgs; ++i)
                {
                    std::string argReg = "a" + std::to_string(i);
                    std::string srcReg = loadOperand(TACOperand::var(paramQueue_[i]), argReg);
                    if (srcReg != argReg)
                        emit("mv " + argReg + ", " + srcReg);
                }
                // 额外参数 (>=8)：使用 maxFrameSize 缓冲区
                for (int i = 8; i < argCount; ++i)
                {
                    std::string srcReg = loadOperand(TACOperand::var(paramQueue_[i]), "t0");
                    if (srcReg != "t0")
                        emit("mv t0, " + srcReg);
                    emitStackStore("t0", maxFrameSize + (i - 8) * 4);
                }
                paramQueue_.clear();

                // call 会 clobber t0-t6 和 a0-a7（caller-saved）
                // s 寄存器是 callee-saved，不受影响
                // 重置 peephole 状态
                lastStoreValid_ = false;
                lastLineValid_ = false;

                // 发射 call 指令（main 函数不加 func_ 前缀）
                if (instr.label == "main")
                    emit("call main");
                else
                    emit("call func_" + instr.label);

                // 如有返回值，存储到 result
                if (instr.result.type != TACOpType::NONE)
                {
                    // 如果结果在 s 寄存器中，storeOperand 会 emit "mv sN, a0"
                    // 记录此 mv，用于消除后续 RETURN 中的 "mv a0, sN" 冗余
                    bool wasInSReg = ((instr.result.type == TACOpType::VAR || instr.result.type == TACOpType::TEMP) &&
                                      varToReg_.count(instr.result.name));
                    storeOperand(instr.result, "a0");
                    if (wasInSReg && varToReg_.count(instr.result.name))
                    {
                        lastMvFromA0_ = varToReg_[instr.result.name];
                        lastMvFromA0Valid_ = true;
                    }
                }
                break;
            }

            case TACType::NOP:
                break;

            default:
                break;
            }
        }

        // 函数末尾无显式 return，补尾声
        // 若最后一条指令已是 ret，则跳过不可达的 fallback epilogue（死代码消除）
        if (!currentFunc_.empty() && !lastEmittedWasRet_)
        {
            emit("li a0, 0");
            emitFuncEpilogue();
        }
    }

    // ================================================================
    //  辅助方法
    // ================================================================

    void CodeGen::emit(const std::string &line)
    {
        std::cout << line << std::endl;
        // 跟踪最后一条是否为 ret（用于跳过不可达 fallback epilogue）
        lastEmittedWasRet_ = (line == "ret");
        // 任何 emit 调用都会使跨寄存器 sw→lw peephole 的"紧邻"条件失效
        // (emitStackStore 在 emit 后会重新设置 lastStoreValid_ = true)
        lastStoreValid_ = false;
        // 跟踪上一条指令用于 peephole；标签/控制流相关指令使跟踪失效
        lastMvFromA0Valid_ = false;
        if (!line.empty() && line.back() == ':')
        {
            // 标签行：控制流汇合点，使跟踪失效
            lastLineValid_ = false;
            lastEmittedLine_.clear();
            return;
        }
        // 控制流指令也使跟踪失效（避免跨基本块误删）
        if (line.rfind("j ", 0) == 0 || line.rfind("bnez", 0) == 0 ||
            line.rfind("beqz", 0) == 0 || line.rfind("beq", 0) == 0 ||
            line.rfind("bne", 0) == 0 || line.rfind("blt", 0) == 0 ||
            line.rfind("bge", 0) == 0 || line.rfind("ble", 0) == 0 ||
            line.rfind("bgt", 0) == 0 || line.rfind("ret", 0) == 0 ||
            line.rfind("call", 0) == 0 || line.rfind("ecall", 0) == 0)
        {
            lastLineValid_ = false;
            lastEmittedLine_.clear();
            return;
        }
        lastEmittedLine_ = line;
        lastLineValid_ = true;
    }

    // 带大偏移的栈加载：offset 超 12 位范围时用 li+add 两步法
    // Peephole 优化：
    //   1. 若上一条刚发射的是 "sw reg, offset(sp)" 且寄存器相同 → 跳过 load
    //   2. 若上一条刚发射的是 "sw regX, offset(sp)" 且寄存器不同 → 替换为 "mv reg, regX"
    void CodeGen::emitStackLoad(const std::string &reg, int offset)
    {
        if (offset >= -2048 && offset <= 2047)
        {
            std::string loadLine = "lw " + reg + ", " + std::to_string(offset) + "(sp)";
            // 优先检查增强peephole（跨寄存器 sw→lw 消除）
            if (lastStoreValid_ && lastStoreOffset_ == offset)
            {
                if (lastStoreReg_ == reg)
                {
                    // 同寄存器：跳过 load（值已在 reg 中）
                    lastStoreValid_ = false;
                    lastLineValid_ = false;
                    lastEmittedLine_.clear();
                    return;
                }
                else
                {
                    // 跨寄存器：用 mv 替代 lw（更高效）
                    lastStoreValid_ = false;
                    emit("mv " + reg + ", " + lastStoreReg_);
                    return;
                }
            }
            // 兼容旧peephole（检查 lastEmittedLine_ 是否完全匹配）
            if (lastLineValid_ && lastEmittedLine_ == "sw " + reg + ", " + std::to_string(offset) + "(sp)")
            {
                lastLineValid_ = false;
                lastEmittedLine_.clear();
                return; // 跳过冗余 load
            }
            emit(loadLine);
        }
        else
        {
            emit("li t2, " + std::to_string(offset));
            emit("add t2, sp, t2");
            emit("lw " + reg + ", 0(t2)");
        }
    }

    void CodeGen::emitStackStore(const std::string &reg, int offset)
    {
        if (offset >= -2048 && offset <= 2047)
        {
            emit("sw " + reg + ", " + std::to_string(offset) + "(sp)");
            // 记录此次 store 供 emitStackLoad 做跨寄存器 peephole
            // 注意：emit() 已将 lastStoreValid_ 置 false，此处恢复为 true
            lastStoreReg_ = reg;
            lastStoreOffset_ = offset;
            lastStoreValid_ = true;
        }
        else
        {
            emit("li t2, " + std::to_string(offset));
            emit("add t2, sp, t2");
            emit("sw " + reg + ", 0(t2)");
            lastStoreValid_ = false;
        }
    }

    std::string CodeGen::loadOperand(const TACOperand &op, const std::string &reg)
    {
        switch (op.type)
        {
        case TACOpType::CONST_INT:
            emit("li " + reg + ", " + std::to_string(op.intValue));
            break;
        case TACOpType::VAR:
            if (isGlobal(op.name))
            {
                emit("la " + reg + ", _g_" + op.name);
                emit("lw " + reg + ", 0(" + reg + ")");
            }
            else if (varToReg_.count(op.name))
            {
                return varToReg_[op.name];
            }
            else
            {
                int offset = allocVarOffset(op.name);
                emitStackLoad(reg, offset);
            }
            break;
        case TACOpType::TEMP:
            if (varToReg_.count(op.name))
            {
                return varToReg_[op.name];
            }
            else
            {
                int offset = allocVarOffset(op.name);
                emitStackLoad(reg, offset);
            }
            break;
        case TACOpType::NONE:
            emit("li " + reg + ", 0");
            break;
        }
        return reg;
    }

    void CodeGen::storeOperand(const TACOperand &op, const std::string &reg)
    {
        switch (op.type)
        {
        case TACOpType::VAR:
            if (isGlobal(op.name))
            {
                emit("la t2, _g_" + op.name);
                emit("sw " + reg + ", 0(t2)");
            }
            else if (varToReg_.count(op.name))
            {
                if (varToReg_[op.name] != reg)
                    emit("mv " + varToReg_[op.name] + ", " + reg);
            }
            else
            {
                int offset = allocVarOffset(op.name);
                emitStackStore(reg, offset);
            }
            break;
        case TACOpType::TEMP:
            if (varToReg_.count(op.name))
            {
                if (varToReg_[op.name] != reg)
                    emit("mv " + varToReg_[op.name] + ", " + reg);
            }
            else
            {
                int offset = allocVarOffset(op.name);
                emitStackStore(reg, offset);
            }
            break;
        case TACOpType::CONST_INT:
        case TACOpType::NONE:
            break;
        }
    }

    int CodeGen::allocVarOffset(const std::string &name)
    {
        if (varOffsets_.count(name))
        {
            return varOffsets_[name];
        }
        // 栈向下增长，变量存放在 sp + offset 位置（正偏移）
        int offset = static_cast<int>(varOffsets_.size()) * 4;
        varOffsets_[name] = offset;
        return offset;
    }

    void CodeGen::emitPrologue()
    {
        emit(".text");
        emit(".globl _start");
        emit("_start:");
        emit("addi sp, sp, -256");
        emit("sw ra, 252(sp)");
    }

    void CodeGen::emitFuncPrologue(const std::string &funcName, int frameSize)
    {
        // 为保存的 s 寄存器额外分配空间
        int sRegSpace = static_cast<int>(usedSRegs_.size()) * 4;
        frameSize += sRegSpace;
        if (frameSize < 256)
            frameSize = 256;
        currentFrameSize_ = frameSize;

        emit("");
        if (funcName == "main")
        {
            emit(".globl main");
            emit("main:");
        }
        else
        {
            emit("func_" + funcName + ":");
        }
        // addi 立即数范围 [-2048,2047]，用两步法避免越界
        emit("li t2, " + std::to_string(frameSize));
        emit("sub sp, sp, t2");
        emit("sw ra, " + std::to_string(frameSize - 4) + "(sp)");

        // 保存使用的 s 寄存器
        for (size_t i = 0; i < usedSRegs_.size(); ++i)
        {
            int offset = frameSize - 8 - static_cast<int>(i) * 4;
            emit("sw " + usedSRegs_[i] + ", " + std::to_string(offset) + "(sp)");
        }

        varOffsets_.clear();
    }

    void CodeGen::emitFuncEpilogue()
    {
        int frameSize = currentFrameSize_;
        if (frameSize <= 0)
            frameSize = 256;

        // 恢复使用的 s 寄存器
        for (size_t i = 0; i < usedSRegs_.size(); ++i)
        {
            int offset = frameSize - 8 - static_cast<int>(i) * 4;
            emit("lw " + usedSRegs_[i] + ", " + std::to_string(offset) + "(sp)");
        }

        emit("lw ra, " + std::to_string(frameSize - 4) + "(sp)");
        // 用 li + add 代替 addi，支持大立即数
        emit("li t2, " + std::to_string(frameSize));
        emit("add sp, sp, t2");
        emit("ret");
    }

    void CodeGen::emitExit()
    {
        emit("li a7, 93");
        emit("ecall");
    }

    bool CodeGen::isGlobal(const std::string &name) const
    {
        return globalVars_.count(name) > 0;
    }

} // namespace MyCompiler
