#include "SymbolTable.h"

namespace MyCompiler {

SymbolTable::SymbolTable() {
    // TODO: 创建全局作用域（scope level 0），scopes_.push_back({});
    scopes_.push_back({});  // 创建全局作用域（scope level 0）
}

void SymbolTable::enterScope() {
    // TODO: scopes_.push_back({});
    scopes_.push_back({});  //创建n级作用域，n为当前栈的大小
}

void SymbolTable::exitScope() {
    // TODO: if (scopes_.size() > 1) scopes_.pop_back();
    if (scopes_.size() > 1){    //确保不会删除全局作用域,即0级作用域
        scopes_.pop_back();
    }
}

bool SymbolTable::define(const Symbol& sym) {
    // TODO: 检查当前作用域是否已有同名符号，没有则插入
    auto &currentScope = scopes_.back();
    if (currentScope.find(sym.name) != currentScope.end()){
        return false;   //当前作用域已有同名符号，返回false
    }
    else {
        currentScope[sym.name] = sym;  //插入符号
        return true;
    }
}

Symbol* SymbolTable::lookup(const std::string& name) {
    // TODO: 从栈顶向下遍历（rbegin → rend），找到则返回 &symbol
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it){//rbegin()返回指向最后一个元素的反向迭代器，rend()返回指向第一个元素前的反向迭代器————把 vector 倒过来遍历
        auto symIt = it->find(name);
        if (symIt != it->end()){
            return &(symIt->second);   //找到符号，返回指针
        }
    }
    return nullptr;
}

Symbol* SymbolTable::lookupCurrent(const std::string& name) {
    // TODO: 仅在栈顶作用域查找
    auto &currentScope = scopes_.back();
    auto symIt = currentScope.find(name);
    if (symIt != currentScope.end()){
        return &(symIt->second);   //找到符号，返回指针
    }
    return nullptr;
}

} // namespace MyCompiler
