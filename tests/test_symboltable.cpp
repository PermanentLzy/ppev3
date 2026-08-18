#include <gtest/gtest.h>
#include "../src/semantic/SymbolTable.h"

using namespace MyCompiler;

// ================================================================
//  符号表测试 — 作用域栈 + 符号定义/查找
// ================================================================

// ---- 1. 基本定义和查找 ----

TEST(SymbolTableTest, DefineAndLookup) {
    SymbolTable st;
    Symbol sym{"x", DataType::INT, 0, true, false, false};
    EXPECT_TRUE(st.define(sym));

    auto* found = st.lookup("x");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "x");
    EXPECT_EQ(found->type, DataType::INT);
    EXPECT_TRUE(found->initialized);
}

TEST(SymbolTableTest, LookupUndefined) {
    SymbolTable st;
    EXPECT_EQ(st.lookup("nonexistent"), nullptr);
}

TEST(SymbolTableTest, DuplicateInSameScope) {
    SymbolTable st;
    Symbol sym1{"x", DataType::INT, 0, true, false, false};
    Symbol sym2{"x", DataType::INT, 0, true, false, false};
    EXPECT_TRUE(st.define(sym1));
    EXPECT_FALSE(st.define(sym2));  // 同作用域重复定义
}

// ---- 2. 作用域嵌套 ----

TEST(SymbolTableTest, NestedScope) {
    SymbolTable st;
    Symbol outer{"x", DataType::INT, 0, true, false, false};
    st.define(outer);

    st.enterScope();
    Symbol inner{"x", DataType::INT, 1, true, false, false};
    EXPECT_TRUE(st.define(inner));  // 内层可定义同名符号

    auto* found = st.lookup("x");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->scopeLevel, 1);  // 内层屏蔽外层

    st.exitScope();

    found = st.lookup("x");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->scopeLevel, 0);  // 恢复外层
}

TEST(SymbolTableTest, LookupCurrent) {
    SymbolTable st;
    Symbol outer{"a", DataType::INT, 0, true, false, false};
    st.define(outer);

    st.enterScope();
    Symbol inner{"b", DataType::INT, 1, true, false, false};
    st.define(inner);

    // lookupCurrent 只看当前作用域
    EXPECT_NE(st.lookupCurrent("b"), nullptr);
    EXPECT_EQ(st.lookupCurrent("a"), nullptr);  // a 在外层

    // lookup 从内向外
    EXPECT_NE(st.lookup("a"), nullptr);
    EXPECT_NE(st.lookup("b"), nullptr);

    st.exitScope();
}

TEST(SymbolTableTest, MultiLevelScope) {
    SymbolTable st;
    st.enterScope();  // level 1
    st.enterScope();  // level 2
    st.enterScope();  // level 3
    EXPECT_EQ(st.currentLevel(), 3);

    st.exitScope();
    EXPECT_EQ(st.currentLevel(), 2);

    st.exitScope();
    EXPECT_EQ(st.currentLevel(), 1);

    st.exitScope();
    EXPECT_EQ(st.currentLevel(), 0);
}

TEST(SymbolTableTest, CannotExitGlobalScope) {
    SymbolTable st;
    EXPECT_EQ(st.currentLevel(), 0);
    st.exitScope();  // 不应崩溃
    EXPECT_EQ(st.currentLevel(), 0);
}

// ---- 3. Const 符号 ----

TEST(SymbolTableTest, ConstSymbol) {
    SymbolTable st;
    Symbol sym{"C", DataType::INT, 0, true, true, false};
    st.define(sym);

    auto* found = st.lookup("C");
    ASSERT_NE(found, nullptr);
    EXPECT_TRUE(found->isConst);
}

// ---- 4. 函数符号 ----

TEST(SymbolTableTest, FunctionSymbol) {
    SymbolTable st;
    Symbol sym;
    sym.name = "main";
    sym.type = DataType::INT;
    sym.scopeLevel = 0;
    sym.initialized = true;
    sym.isFunction = true;
    sym.returnType = DataType::INT;
    sym.paramTypes = {};
    st.define(sym);

    auto* found = st.lookup("main");
    ASSERT_NE(found, nullptr);
    EXPECT_TRUE(found->isFunction);
    EXPECT_EQ(found->returnType, DataType::INT);
    EXPECT_TRUE(found->paramTypes.empty());
}

TEST(SymbolTableTest, FunctionWithParams) {
    SymbolTable st;
    Symbol sym;
    sym.name = "add";
    sym.type = DataType::INT;
    sym.scopeLevel = 0;
    sym.initialized = true;
    sym.isFunction = true;
    sym.returnType = DataType::INT;
    sym.paramTypes = {DataType::INT, DataType::INT};
    st.define(sym);

    auto* found = st.lookup("add");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->paramTypes.size(), 2u);
}

TEST(SymbolTableTest, AtGlobalScope) {
    SymbolTable st;
    EXPECT_TRUE(st.atGlobalScope());

    st.enterScope();
    EXPECT_FALSE(st.atGlobalScope());

    st.exitScope();
    EXPECT_TRUE(st.atGlobalScope());
}
