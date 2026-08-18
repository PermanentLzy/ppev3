/**
 * @file main.cpp
 * @brief ToyC 编译器入口 —— stdin 读取 → 编译 → stdout 输出 RISC-V 32 汇编
 *
 * 编译流程（9 个阶段）：
 * 1. 词法分析（Lexer）    —— 字符流 → Token 流
 * 2. 语法分析（Parser）   —— Token 流 → AST
 * 3. AST 打印（调试）    —— 输出树形结构 (DEBUG_AST=1 时启用)
 * 4. 符号表（SymbolTable）—— 作用域栈，支撑语义分析
 * 5. 语义分析（Semantic） —— 类型检查 + 作用域 + 函数检查
 * 6. IR 生成（IRGen）    —— AST → 三地址码
 * 7. 优化（Optimizer）    —— 常量折叠/CSE/死代码删除 (-opt 时启用)
 * 8. 代码生成（CodeGen）  —— 三地址码 → RISC-V 32 汇编 → stdout
 * 9. 串联（main.cpp）    —— 组装全部阶段，端到端运行
 *
 * 用法:
 * mycompiler < input.tc > output.s        (基础编译)
 * mycompiler -opt < input.tc > output.s    (开启优化)
 */

 #include "lexer/Lexer.h"
 #include "parser/Parser.h"
 #include "parser/ParseError.h"
 #include "ast/AST.h"
 #include "semantic/SemanticAnalyzer.h"
 #include "ir/IRGenerator.h"
 #include "optimizer/Optimizer.h"
 #include "codegen/CodeGen.h"
 #include "utils/ErrorHandler.h"
 
 #include <iostream>
 #include <sstream>
 #include <string>
 #include <cstring>
 
 using namespace MyCompiler;
 
 // ---- 声明（定义在 ASTPrinter.cpp）----
 namespace MyCompiler
 {
     void printAST(Program &prog);
 }
 
 // ================================================================
 int main(int argc, char *argv[])
 {
     // ---- 解析 -opt 参数 ----
     bool enableOpt = false;
     for (int i = 1; i < argc; ++i)
     {
         if (std::strcmp(argv[i], "-opt") == 0)
         {
             enableOpt = true;
         }
     }
 
     // ---- 从 stdin 读取源程序 ----
     std::ostringstream oss;
     oss << std::cin.rdbuf();
     std::string source = oss.str();
     if (source.empty())
     {
         std::cerr << "[Error] Empty input.\n";
         return 1;
     }
 
     // ---- 阶段 1：词法分析 ----
     Lexer lexer(source);
 
     // ---- 阶段 2：语法分析 ----
     Parser parser(lexer);
     std::unique_ptr<Program> ast;
     try
     {
         ast = parser.parse();
     }
     catch (const ParseError &e)
     {
         std::cerr << e.what();
         return 1;
     }
 
     // ---- 阶段 3：AST 打印（调试，默认关闭）----
     if (std::getenv("DEBUG_AST"))
     {
         printAST(*ast);
     }
 
     // ---- 阶段 4 + 5：语义分析（内部使用符号表）----
     SemanticAnalyzer sema;
     if (sema.analyze(*ast) > 0)
     {
         std::cerr << "Semantic errors\n";
         return 1;
     }
 
     // ---- 阶段 6：IR 生成 ----
     IRGenerator irGen;
     auto irProgram = irGen.generate(*ast);
     if (std::getenv("DEBUG_IR"))
     {
         irProgram->print();
     }
 
     // ---- 阶段 7：优化（-opt 时启用）----
     if (enableOpt)
     {
         Optimizer opt;
         opt.optimize(*irProgram);
         if (std::getenv("DEBUG_IR"))
         {
             std::cerr << "=== IR AFTER OPTIMIZATION ===\n";
             irProgram->print();
         }
     }
 
     // ---- 阶段 8：代码生成 → RISC-V 32 汇编 → stdout ----
     CodeGen codeGen;
     codeGen.generate(*irProgram);
 
     return 0;
 }