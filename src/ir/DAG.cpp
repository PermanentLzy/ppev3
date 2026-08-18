#include "DAG.h"
#include <iostream>
#include <string>

namespace MyCompiler {

std::string DAG::makeKey(const std::string& label, int left, int right) const {//用于生成唯一键值，便于在缓存中查找节点
    // TODO: return label + "|" + left + "|" + right
    std::string key = label + "|" + std::to_string(left) + "|" + std::to_string(right);
    return key;
}

int DAG::getLeaf(const std::string& varName) {
    // TODO: 在 cache_ 中查找 "VAR_" + varName，命中则返回已有 ID
    //       否则创建新 DAGNode，插入 cache_ 并返回新 ID
    auto it = cache_.find("VAR_" + varName);
    if (it != cache_.end()) {
        return it->second; // 返回已有 ID
    } else {
        int newId = nodes_.size();
        DAGNode newNode;
        newNode.id = newId;
        newNode.label = "VAR";
        newNode.isLeaf = true;
        newNode.varName = varName;
        nodes_.push_back(newNode);
        cache_["VAR_" + varName] = newId; // 插入缓存
        return newId; // 返回新 ID
    }
}

int DAG::getLeaf(int constantValue) {
    // TODO: 在 cache_ 中查找 "INT_" + value，命中则返回已有 ID
    //       否则创建新 DAGNode，插入 cache_ 并返回新 ID
    auto it = cache_.find("INT_" + std::to_string(constantValue));
    if (it != cache_.end()) {
        return it->second; // 返回已有 ID
    } else {
        int newId = nodes_.size();
        DAGNode newNode;
        newNode.id = newId;
        newNode.label = "INT";
        newNode.isLeaf = true;
        newNode.intValue = constantValue;
        nodes_.push_back(newNode);
        cache_["INT_" + std::to_string(constantValue)] = newId; // 插入缓存
        return newId; // 返回新 ID
    }
    return 0;
}

int DAG::getNode(const std::string& op, int leftId, int rightId) {
    // TODO: 用 makeKey 查 cache_
    //       命中 → 返回已有 ID（共享子表达式）
    //       未命中 → 创建新节点，插入 cache_
    auto key = makeKey(op, leftId, rightId);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        return it->second; // 返回已有 ID
    } else {
        int newId = nodes_.size();
        DAGNode newNode;
        newNode.id = newId;
        newNode.label = op;
        newNode.leftChild = leftId;
        newNode.rightChild = rightId;
        nodes_.push_back(newNode);
        cache_[key] = newId; // 插入缓存
        return newId; // 返回新 ID
    }
    return 0;
}

void DAG::print() const {
    // TODO: 遍历 nodes_ 打印每个节点的信息
    auto node = nodes_.begin();
    while(node != nodes_.end()){
        std::cout << "Node ID: " << node->id << ", Label: " << node->label;
        if (node->isLeaf) {
            if (node->label == "VAR") {
                std::cout << ", Variable Name: " << node->varName;
            } else if (node->label == "INT") {
                std::cout << ", Constant Value: " << node->intValue;
            }
        } else {
            std::cout << ", Left Child ID: " << node->leftChild << ", Right Child ID: " << node->rightChild;
        }
        std::cout << std::endl;
        ++node;
    }
}

} // namespace MyCompiler
