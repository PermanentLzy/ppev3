#pragma once

#include "../ast/AST.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace MyCompiler {

/// @brief 符号信息
struct Symbol {
    std::string name;
    DataType    type;
    int         scopeLevel;  // 作用域层级（0 表示全局作用域）
    bool        initialized; // 是否已赋值
    bool        isConst = false;     // 是否为 const 常量
    bool        isFunction = false;  // 是否为函数

    // 以下字段仅对函数有效
    DataType    returnType = DataType::VOID;
    std::vector<DataType> paramTypes; // 参数类型列表
    bool        hasReturnOnAllPaths = false; // TODO: 用于检查 int 函数所有路径是否 return
};

/// @brief 符号表：用栈模拟作用域嵌套（最内层在栈顶）
class SymbolTable {
public:
    SymbolTable();

    /// 进入新作用域（遇到 '{' 时调用）
    void enterScope();

    /// 退出当前作用域（遇到 '}' 时调用）
    void exitScope();

    /// 在当前作用域定义符号
    /// @returns false 若当前作用域已有同名符号
    bool define(const Symbol& sym);

    /// 从内向外查找符号（内层屏蔽外层）
    /// @returns nullptr 若未找到
    Symbol* lookup(const std::string& name);

    /// 在当前作用域查找（不向外层查）
    Symbol* lookupCurrent(const std::string& name);

    /// 获取当前作用域层级
    int currentLevel() const { return static_cast<int>(scopes_.size()) - 1; }

    /// 是否在全局作用域
    bool atGlobalScope() const { return scopes_.size() == 1; }

private:
    std::vector<std::unordered_map<std::string, Symbol>> scopes_;       //作用域栈，每个作用域是一个符号名 -> 符号信息的哈希表
};

} // namespace MyCompiler
