#ifndef XSF_SKIP_LIST_H
#define XSF_SKIP_LIST_H

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>

namespace xsf_skip_list {

std::mutex mtx;

template <typename K, typename V>
class Node {
   public:
    Node() {}
    Node(K key, V value, int level);
    ~Node();
    K get_key() const;
    V get_value() const;
    void set_value(V value);

    Node<K, V>** forward_;
    int node_level_;

   private:
    K key_;
    V value_;
};

template <typename K, typename V>
Node<K, V>::Node(K key, V value, int level)
    : key_(key), value_(value), node_level_(level) {
    forward_ = new Node<K, V>*[level + 1];
    memset(forward_, 0, sizeof(Node<K, V>*) * (level + 1));
}

template <typename K, typename V>
Node<K, V>::~Node() {
    delete[] forward_;
}

template <typename K, typename V>
K Node<K, V>::get_key() const {
    return key_;
}

template <typename K, typename V>
V Node<K, V>::get_value() const {
    return value_;
}

template <typename K, typename V>
void Node<K, V>::set_value(V value) {
    value_ = value;
}

template <typename K, typename V>
class XSFSkipList {
   public:
    XSFSkipList(int max_level);
    ~XSFSkipList();
    int insert_element(K key, V value);    // 插入元素
    bool search_element(K key, V& value);  // 查找元素
    void delete_element(K key);            // 删除元素
    int size();                            // 获取跳表元素个数

   private:
    int get_random_level();                              // 获取随机层数
    Node<K, V>* create_node(K key, V value, int level);  // 创建节点
    bool is_valid_string(const std::string& str);        // 验证字符串的合法性
    void get_key_value_from_string(const std::string& str, std::string* key,
                                   std::string* value);  // 从字符串中获取键值对
    void clear(Node<K, V>* node);                        // 清空跳表

    int max_level_;        // 层数最大值
    int skip_list_level_;  // 跳表当前最高层数
    Node<K, V>* header_;   // 跳表头结点
    int element_count_;    // 跳表元素个数
};

// 初始化跳表
template <typename K, typename V>
XSFSkipList<K, V>::XSFSkipList(int max_level)
    : max_level_(max_level), skip_list_level_(0), element_count_(0) {
    // 创建头节点，并初始化键值为默认值
    K k;
    V v;
    header_ = new Node<K, V>(k, v, max_level_);
}

// 销毁跳表
template <typename K, typename V>
XSFSkipList<K, V>::~XSFSkipList() {
    // 递归删除跳表链条
    if (header_->forward_[0] != nullptr) {
        clear(header_->forward_[0]);
    }
    // 删除头节点
    delete header_;
}

// 获取随机层数
template <typename K, typename V>
int XSFSkipList<K, V>::get_random_level() {
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

/**
 * 创建一个新节点
 * @param k 节点的键
 * @param v 节点的值
 * @param level 节点的层级
 * @return 新创建的节点指针
 */
template <typename K, typename V>
Node<K, V>* XSFSkipList<K, V>::create_node(K k, V v, int level) {
    // 实例化新节点，并为其分配指定的键、值和层级
    Node<K, V>* new_node = new Node<K, V>(k, v, level);
    // 返回新节点：返回新节点指针
    return new_node;
}

/**
 * 搜索指定的键值是否存在于跳表中。
 * @param key 待查找的键值
 * @param value 如果找到键值，返回键值对应的值
 * @return 如果找到键值，返回 true；否则返回 false。
 */
template <typename K, typename V>
bool XSFSkipList<K, V>::search_element(K key, V& value) {
    // 定义一个指针 current，初始化为跳表的头节点
    Node<K, V>* current = header_;

    // 从跳表的最高层开始搜索
    for (int i = skip_list_level_; i >= 0; i--) {
        // 遍历当前层级，直到下一个节点的键值大于或等于待查找的键值
        while (current->forward_[i] != nullptr &&
               current->forward_[i]->get_key() < key) {
            // 移动到当前层的下一个节点
            current = current->forward_[i];
        }
        // 当前节点的下一个节点的键值大于待查找的键值时，进行下沉到下一层
        // 下沉操作通过循环的 i-- 实现
    }

    // 检查当前层（最底层）的下一个节点的键值是否为待查找的键值
    current = current->forward_[0];
    if (current != nullptr && current->get_key() == key) {
        // 如果找到匹配的键值，返回 true
        value = current->get_value();
        return true;
    }
    // 如果没有找到匹配的键值，返回 false
    return false;
}

/**
 * 在跳表中插入一个新元素。
 * @param key 待插入节点的 key
 * @param value 待插入节点的 value
 * @return 如果元素已存在，返回 1；否则，进行更新 value 操作并返回 0。
 */
template <typename K, typename V>
int XSFSkipList<K, V>::insert_element(const K key, const V value) {
    mtx.lock();
    // 定义一个指针 current，初始化为跳表的头节点
    Node<K, V>* current = header_;
    // 用于在各层更新指针的数组
    Node<K, V>* update[max_level_ + 1];  // 用于记录每层中待更新指针的节点
    memset(update, 0, sizeof(Node<K, V>*) * (max_level_ + 1));

    // 从最高层向下搜索插入位置
    for (int i = skip_list_level_; i >= 0; i--) {
        // 寻找当前层中最接近且小于 key 的节点
        while (current->forward_[i] != nullptr &&
               current->forward_[i]->get_key() < key) {
            // 移动到下一节点
            current = current->forward_[i];
        }
        // 保存每层中该节点，以便后续插入时更新指针
        update[i] = current;
    }

    // 移动到最底层的下一节点，准备插入操作
    current = current->forward_[0];
    // 检查待插入的节点的键是否已存在
    if (current != nullptr && current->get_key() == key) {
        // 键已存在，更新节点的值
        current->set_value(value);
        // 返回 1，表示更新操作
        mtx.unlock();
        return 1;
    } else {
        // 键不存在，插入节点
        // 通过随机函数决定新节点的层级高度
        int new_node_level = get_random_level();
        // 如果新节点的层级超出了跳表的当前最高层级
        if (new_node_level > skip_list_level_) {
            // 对所有新的更高层级，将头节点设置为它们的前驱节点
            for (int i = skip_list_level_ + 1; i < new_node_level + 1; i++) {
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
        mtx.unlock();
        return 0;
    }
}

/**
 * 删除跳表中的节点
 * @param key 待删除节点的 key 值
 */
template <typename K, typename V>
void XSFSkipList<K, V>::delete_element(K key) {
    mtx.lock();
    // 定义一个指针 current，初始化为跳表的头节点
    Node<K, V>* current = header_;
    // 用于在各层更新指针的数组
    Node<K, V>* update[max_level_ + 1];  // 用于记录每层中待更新指针的节点
    memset(update, 0, sizeof(Node<K, V>*) * (max_level_ + 1));

    // 从最高层开始向下搜索待删除节点
    for (int i = skip_list_level_; i >= 0; i--) {
        while (current->forward_[i] != nullptr &&
               current->forward_[i]->get_key() < key) {
            current = current->forward_[i];
        }
        // 记录每层待删除节点的前驱节点
        update[i] = current;
    }
    current = current->forward_[0];
    // 确认找到了待删除的节点
    if (current != nullptr && current->get_key() == key) {
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
    mtx.unlock();
}

// 验证字符串的合法性
template <typename K, typename V>
bool XSFSkipList<K, V>::is_valid_string(const std::string& str) {
    return !str.empty() && str.find(':') != std::string::npos;
}

// 从字符串中获取键值对
template <typename K, typename V>
void XSFSkipList<K, V>::get_key_value_from_string(const std::string& str,
                                                  std::string* key,
                                                  std::string* value) {
    if (!is_valid_string(str)) {
        return;
    }
    *key = str.substr(0, str.find(':'));
    *value = str.substr(str.find(':') + 1);
}

// 清空跳表
template <typename K, typename V>
void XSFSkipList<K, V>::clear(Node<K, V>* node) {
    if (node->forward_[0] != nullptr) {
        clear(node->forward_[0]);
    }
    delete node;
}

// 获取跳表元素个数
template <typename K, typename V>
int XSFSkipList<K, V>::size() {
    return element_count_;
}

}  // namespace xsf_skip_list

#endif  // XSF_SKIP_LIST_H