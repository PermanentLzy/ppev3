# MyCompiler —— 简易编译器

基于 C++17 构建的教学用简易编译器，支持词法分析、语法分析、语义分析、中间代码生成、优化和代码生成。

## 构建

```bash
mkdir build && cd build
cmake ..
cmake --build . -j4
```

## 运行

```bash
./mycompiler ../examples/arith.src
```

## 测试

```bash
cd build
ctest --output-on-failure
```

## 项目结构

```
src/
├── lexer/      词法分析器
├── parser/     递归下降语法分析器
├── ast/        抽象语法树定义
├── semantic/   语义分析 & 符号表
├── ir/         中间代码生成（三地址码 / DAG）
├── optimizer/  IR 优化（常量折叠、CSE）
├── codegen/    代码生成（解释执行）
└── utils/      错误报告等工具
```
