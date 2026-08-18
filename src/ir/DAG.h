#pragma once

#include <vector>
#include <string>
#include <unordered_map>

namespace MyCompiler {

/// @brief DAG 节点
struct DAGNode {
    int id;                // 唯一标识
    std::string label;     // 运算符或变量名
    int leftChild  = -1;   // 左子节点 ID（-1 表示无）
    int rightChild = -1;   // 右子节点 ID（-1 表示无）
    bool isLeaf    = false;
    std::string varName;   // 叶节点变量名
    int intValue   = 0;    // 叶节点常量值（若是常量）
};

/// @brief DAG 结构
class DAG {
public:
    /// 添加或获取叶节点（变量/常量）
    int getLeaf(const std::string& varName);
    int getLeaf(int constantValue);

    /// 添加或获取内部节点（二元运算）
    int getNode(const std::string& op, int leftId, int rightId);

    /// 打印 DAG
    void print() const;

    const std::vector<DAGNode>& nodes() const { return nodes_; }

private:
    std::vector<DAGNode> nodes_;

    /// 哈希表：键 = "label|leftChild|rightChild"，值 = 节点 ID
    std::unordered_map<std::string, int> cache_;

    std::string makeKey(const std::string& label, int left, int right) const;
};

} // namespace MyCompiler
