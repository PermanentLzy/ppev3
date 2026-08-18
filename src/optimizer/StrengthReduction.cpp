#include "Optimizer.h"
#include <iostream>
#include <unordered_map>
#include <cmath>

namespace MyCompiler {

// ================================================================
//  强度削弱优化（Strength Reduction）
//  
//  原理：
//    - 将昂贵操作转化为便宜操作
//    - 乘以 2^n → 左移 n 位（更快）
//    - 乘以 3 → a + (a << 1)（有时更快）
//    - 除以 2^n → 右移 n 位
//    - 循环中的 induction 变量优化（后续扩展）
// ================================================================

void Optimizer::strengthReduction(TACProgram& program) {
    for (auto& instr : program.instructions) {
        if (instr.type != TACType::BINARY) continue;
        
        // 乘法优化：x * 2^n → x << n
        if (instr.op == "*" && instr.rhs.type == TACOpType::CONST_INT) {
            int multiplier = instr.rhs.intValue;
            
            // 检查是否是 2 的幂
            if (multiplier > 0 && (multiplier & (multiplier - 1)) == 0) {
                // 计算 log2(multiplier)
                int shift = 0;
                int temp = multiplier;
                while (temp > 1) {
                    temp >>= 1;
                    ++shift;
                }
                
                // 替换为左移
                instr.op = "<<";
                instr.rhs.intValue = shift;
                std::cerr << "[StrengthRed] 乘以 " << multiplier 
                         << " → 左移 " << shift << " 位\n";
            }
        }
        // 除法优化：保留 / 运算符，由 CodeGen 正确处理带符号除法
        // CodeGen 对 / 2^n 已实现了带 bias 的算术右移（正确舍入）
        // 此处不转换为 >>，因为 C 除法向零截断，而 >> 向负无穷截断
    }
}

} // namespace MyCompiler
