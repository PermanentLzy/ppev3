#pragma once

#include "../ir/ThreeAddrCode.h"
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

namespace MyCompiler
{

    /// @brief RISC-V 32 汇编代码生成器
    /// @note  将三地址码 (TAC) 转换为 RISC-V 32 汇编并输出到 stdout
    ///
    /// 调用约定参考:
    ///   - a0-a7 (x10-x17): 函数参数 / 返回值
    ///   - ra (x1):  返回地址
    ///   - sp (x2):  栈指针
    ///   - t0-t2 (x5-x7): 临时寄存器
    class CodeGen
    {
    public:
        CodeGen() = default;

        /// 将 TAC 程序编译为 RISC-V 32 汇编，输出到 stdout
        void generate(const TACProgram &program);

    private:
        /// 变量/临时变量 → 栈偏移映射（每个函数独立）
        std::unordered_map<std::string, int> varOffsets_;

        /// 标签 → RISC-V 汇编标签映射
        std::unordered_map<std::string, std::string> labelMap_;

        /// 所有函数名集合（从 CALL 指令中收集）
        std::unordered_set<std::string> funcNames_;

        /// 当前处理的函数名（空串 = 程序级别）
        std::string currentFunc_;

        /// PARAM 队列：收集 CALL 前的参数
        std::vector<std::string> paramQueue_;

        /// FUNC_ARG 计数器：当前函数中已处理的参数序号
        int funcArgIndex_ = 0;

        /// 当前函数是否已经 return（之后只输出标签）
        bool funcReturned_ = false;

        /// 当前函数已分配的栈帧大小（字节数）
        int currentFrameSize_ = 0;

        /// Peephole 优化：跟踪上一条发射的指令，用于消除 "sw reg, X(sp); lw reg, X(sp)" 冗余 load
        std::string lastEmittedLine_;
        /// 标记 lastEmittedLine_ 是否有效（标签/控制流后失效）
        bool lastLineValid_ = false;

        /// 增强peephole：跟踪最近一次 sw 指令的源寄存器和偏移
        /// 用于跨寄存器消除 "sw t0, X(sp); lw a0, X(sp)" → "mv a0, t0"
        std::string lastStoreReg_;
        int lastStoreOffset_ = 0;
        bool lastStoreValid_ = false;

        /// 跟踪最后一条发射的指令是否为 ret（用于跳过函数末尾的不可达 fallback epilogue）
        bool lastEmittedWasRet_ = false;

        /// Peephole: 跟踪最近的 "mv sN, a0" 指令
        /// 用于消除 "mv sN, a0; mv a0, sN" 冗余模式（CALL+RETURN）
        std::string lastMvFromA0_;
        bool lastMvFromA0Valid_ = false;

        /// 寄存器分配：局部变量 → s 寄存器映射（每个函数独立）
        /// 将使用最频繁的局部变量分配到 s0-s11 寄存器，避免栈访问
        std::unordered_map<std::string, std::string> varToReg_;
        /// 当前函数使用的 s 寄存器列表（用于 prologue 保存 / epilogue 恢复）
        std::vector<std::string> usedSRegs_;
        /// 可用的 s 寄存器池
        static const std::vector<std::string> sRegPool_;

        /// 全局变量名集合（程序级 ASSIGN 的 result）
        std::unordered_set<std::string> globalVars_;

        /// 全局变量及其初始值（用于 .data 段）
        std::vector<std::pair<std::string, int>> globalInit_;

        /// 输出一行汇编
        void emit(const std::string &line);

        /// 带大偏移的栈加载（自动处理 12 位立即数溢出）
        void emitStackLoad(const std::string &reg, int offset);
        /// 带大偏移的栈存储
        void emitStackStore(const std::string &reg, int offset);

        /// 将 TAC 操作数加载到寄存器 (返回寄存器名)
        std::string loadOperand(const TACOperand &op, const std::string &reg);

        /// 将寄存器值存回变量
        void storeOperand(const TACOperand &op, const std::string &reg);

        /// 分配变量栈偏移（仅局部变量）
        int allocVarOffset(const std::string &name);

        /// 判断是否为全局变量
        bool isGlobal(const std::string &name) const;

        /// 输出汇编头部 (.text, _start 入口等)
        void emitPrologue();

        /// 输出函数序言（保存 ra, 分配栈帧）
        void emitFuncPrologue(const std::string &funcName, int frameSize);

        /// 输出函数尾声（恢复 ra, 释放栈帧, ret）
        void emitFuncEpilogue();

        /// 输出程序退出代码 (exit syscall)
        void emitExit();
    };

} // namespace MyCompiler
