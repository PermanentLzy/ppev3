#pragma once

#include "../ir/ThreeAddrCode.h"
#include <memory>

namespace MyCompiler {

/// @brief 优化器：对 TACProgram 执行一系列优化 pass
class Optimizer {
public:
    Optimizer() = default;

    /// 执行所有优化 pass
    void optimize(TACProgram& program);

private:
    /// 常量折叠 + 代数化简
    void constantFolding(TACProgram& program);

    /// 复写传播
    void copyPropagation(TACProgram& program);

    /// 公共子表达式消除
    void commonSubexpressionElimination(TACProgram& program);

    /// 死代码删除
    void deadCodeElimination(TACProgram& program);

    /// 尾递归消除
    void tailRecursionElimination(TACProgram& program);

    /// 不可达代码删除：删除无条件跳转/返回后的死代码块
    void removeUnreachableCode(TACProgram& program);

    /// 临时→局部合并：消除 %t = compute; var = %t 冗余拷贝链
    void coalesceTemporaryCopies(TACProgram& program);

    /// 循环不变式提取（LICM）：将循环内不依赖循环迭代的指令移出循环
    void loopInvariantCodeMotion(TACProgram& program);

    /// 强度削弱：用更高效的操作替换低效操作（如 x*4 → x<<2）
    void strengthReduction(TACProgram& program);

    /// 窥孔优化：识别和优化相邻指令的低效模式
    void peepholeOptimization(TACProgram& program);
};

} // namespace MyCompiler
