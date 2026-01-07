#ifndef XSF_SKIP_LIST_H
#define XSF_SKIP_LIST_H

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>

namespace xsf_skip_list {

template <typename K, typename V>
struct Node {
   public:
    Node() = default;

    Node(K key, V value, int level)
        : key_(key), value_(value), node_level_(level) {
        forward_ = new Node<K, V>*[level + 1];
        memset(forward_, 0, sizeof(Node<K, V>*) * (level + 1));
    }

    ~Node() { delete[] forward_; }

    K key_;
    V value_;
    Node<K, V>** forward_;
    int node_level_;
};

template <typename K, typename V>
class XSFSkipList {
   public:
    XSFSkipList(int max_level)
        : max_level_(max_level), skip_list_level_(0), element_count_(0) {
        // 创建头节点，并初始化键值为默认值
        K k;
        V v;
        header_ = new Node<K, V>(k, v, max_level_);
    }

    ~XSFSkipList() {
        // 递归删除跳表链条
        if (header_->forward_[0] != nullptr) {
            clear(header_->forward_[0]);
        }
        // 删除头节点
        delete header_;
    }

    int insert_element(K key, V value) {
        std::lock_guard<std::mutex> lock(mutex_);
        // 定义一个指针 current，初始化为跳表的头节点
        Node<K, V>* current = header_;
        // 用于在各层更新指针的数组
        Node<K, V>* update[max_level_ + 1];  // 用于记录每层中待更新指针的节点
        memset(update, 0, sizeof(Node<K, V>*) * (max_level_ + 1));

        // 从最高层向下搜索插入位置
        for (int i = skip_list_level_; i >= 0; i--) {
            // 寻找当前层中最接近且小于 key 的节点
            while (current->forward_[i] != nullptr &&
                   current->forward_[i]->key_ < key) {
                // 移动到下一节点
                current = current->forward_[i];
            }
            // 保存每层中该节点，以便后续插入时更新指针
            update[i] = current;
        }

        // 移动到最底层的下一节点，准备插入操作
        current = current->forward_[0];
        // 检查待插入的节点的键是否已存在
        if (current != nullptr && current->key_ == key) {
            // 键已存在，更新节点的值
            current->value_ = value;
            // 返回 1，表示更新操作
            return 1;
        } else {
            // 键不存在，插入节点
            // 通过随机函数决定新节点的层级高度
            int new_node_level = get_random_level();
            // 如果新节点的层级超出了跳表的当前最高层级
            if (new_node_level > skip_list_level_) {
                // 对所有新的更高层级，将头节点设置为它们的前驱节点
                for (int i = skip_list_level_ + 1; i < new_node_level + 1;
                     i++) {
                    update[i] = header_;
                }
                // 更新跳表的当前最高层级为新节点的层级
                skip_list_level_ = new_node_level;
            }
            // 创建新节点
            Node<K, V>* new_node = create_node(key, value, new_node_level);
            // 在各层插入新节点，同时更新前驱节点的forward指针
            for (int i = 0; i <= new_node_level; i++) {
                // 新节点指向当前节点的下一个节点
                new_node->forward_[i] = update[i]->forward_[i];
                // 当前节点的下一个节点更新为新节点
                update[i]->forward_[i] = new_node;
            }
            // 增加跳表的元素计数
            element_count_++;
            // 返回 0，表示插入操作
            return 0;
        }
    }

    bool search_element(K key, V& value) {
        // 定义一个指针 current，初始化为跳表的头节点
        Node<K, V>* current = header_;

        // 从跳表的最高层开始搜索
        for (int i = skip_list_level_; i >= 0; i--) {
            // 遍历当前层级，直到下一个节点的键值大于或等于待查找的键值
            while (current->forward_[i] != nullptr &&
                   current->forward_[i]->key_ < key) {
                // 移动到当前层的下一个节点
                current = current->forward_[i];
            }
            // 当前节点的下一个节点的键值大于待查找的键值时，进行下沉到下一层
            // 下沉操作通过循环的 i-- 实现
        }

        // 检查当前层（最底层）的下一个节点的键值是否为待查找的键值
        current = current->forward_[0];
        if (current != nullptr && current->key_ == key) {
            // 如果找到匹配的键值，返回 true
            value = current->value_;
            return true;
        }
        // 如果没有找到匹配的键值，返回 false
        return false;
    }

    void delete_element(K key) {
        std::lock_guard<std::mutex> lock(mutex_);
        // 定义一个指针 current，初始化为跳表的头节点
        Node<K, V>* current = header_;
        // 用于在各层更新指针的数组
        Node<K, V>* update[max_level_ + 1];  // 用于记录每层中待更新指针的节点
        memset(update, 0, sizeof(Node<K, V>*) * (max_level_ + 1));

        // 从最高层开始向下搜索待删除节点
        for (int i = skip_list_level_; i >= 0; i--) {
            while (current->forward_[i] != nullptr &&
                   current->forward_[i]->key_ < key) {
                current = current->forward_[i];
            }
            // 记录每层待删除节点的前驱节点
            update[i] = current;
        }
        current = current->forward_[0];
        // 确认找到了待删除的节点
        if (current != nullptr && current->key_ == key) {
            // 逐层更新指针，移除节点
            for (int i = 0; i <= skip_list_level_; i++) {
                // 如果当前层的前驱节点指向待删除节点
                if (update[i]->forward_[i] == current) {
                    // 将前驱节点的指针指向待删除节点的下一个节点
                    update[i]->forward_[i] = current->forward_[i];
                }
            }
            // 调整跳表的层级
            while (skip_list_level_ > 0 &&
                   header_->forward_[skip_list_level_] == nullptr) {
                skip_list_level_--;
            }
            // 释放节点占用的内存
            delete current;
            // 减少跳表的元素计数
            element_count_--;
        }
    }

    int size() { return element_count_; }

   private:
    int get_random_level() {
        // 初始化层级：每个节点至少出现在第一层
        int k = 0;
        // 随机层级增加：使用 rand() % 2 实现抛硬币效果，决定是否升层
        // 层级限制：确保节点层级不超过最大值 max_level_
        while (rand() % 2 && k < max_level_) {
            k++;
        }
        // 返回层级：返回确定的层级值，决定节点插入的层
        return k;
    }

    Node<K, V>* create_node(K key, V value, int level) {
        // 实例化新节点，并为其分配指定的键、值和层级
        Node<K, V>* new_node = new Node<K, V>(key, value, level);
        // 返回新节点：返回新节点指针
        return new_node;
    }

    void clear(Node<K, V>* node) {
        if (node->forward_[0] != nullptr) {
            clear(node->forward_[0]);
        }
        delete node;
    }

    int max_level_;
    int skip_list_level_;  // 跳表当前最高层数
    Node<K, V>* header_;
    int element_count_;

    std::mutex mutex_;
};

}  // namespace xsf_skip_list

#endif  // XSF_SKIP_LIST_H