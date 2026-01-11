#ifndef MOMU_SKIP_LIST_H
#define MOMU_SKIP_LIST_H

#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <vector>

namespace momu {
namespace skip_list {

template <typename K, typename V>
struct Node {
   public:
    Node() = default;

    Node(const K& key, const V& value, uint8_t level)
        : key_(key), value_(value), forward_(level + 1, nullptr) {}

    Node(Node&&) noexcept = default;
    Node& operator=(Node&&) noexcept = default;

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    ~Node() = default;

    K key_{};
    V value_{};
    std::vector<Node*> forward_;
};

template <typename K, typename V>
class SkipList {
   public:
    SkipList(uint8_t max_level, unsigned int seed = std::random_device{}())
        : max_level_(max_level),
          header_(create_header_node()),
          gen_(seed),
          distribution_(0.5) {}

    ~SkipList() {}

    SkipList(const SkipList&) = delete;
    SkipList& operator=(const SkipList&) = delete;

    SkipList(SkipList&&) noexcept = default;
    SkipList& operator=(SkipList&&) noexcept = default;

    void put(const K& key, const V& value) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto predecessors = find_predecessors(key);
        auto existing_node = get_node_at_level_zero(predecessors[0], key);

        if (existing_node != nullptr) {
            update_existing_node(existing_node, value);
        } else {
            insert_new_node(key, value, predecessors);
        }
    }

    std::optional<V> get(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto node = find_node(key);
        if (node != nullptr) {
            return node->value_;
        }
        return std::nullopt;
    }

    bool contains(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return find_node(key) != nullptr;
    }

    bool remove(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto predecessors = find_predecessors(key);
        auto node_to_delete = get_node_at_level_zero(predecessors[0], key);

        if (node_to_delete == nullptr) {
            return false;
        }

        delete_node(node_to_delete, predecessors);
        adjust_max_level();
        return true;
    }

    size_t size() { return element_count_; }

    bool empty() const { return element_count_ == 0; }

   private:
    Node<K, V>* create_header_node() {
        return create_node(K{}, V{}, max_level_);
    }

    Node<K, V>* find_node(const K& key) { return traverse_to_level_zero(key); }

    std::vector<Node<K, V>*> find_predecessors(const K& key) {
        std::vector<Node<K, V>*> predecessors(max_level_ + 1, nullptr);
        traverse_and_collect_predecessors(key, predecessors);
        return predecessors;
    }

    Node<K, V>* traverse_to_level_zero(const K& key) {
        Node<K, V>* current = header_;

        for (int i = current_max_level_; i >= 0; i--) {
            current = move_forward_in_level(current, i, key);
        }

        return get_target_node(current, key);
    }

    void traverse_and_collect_predecessors(
        const K& key, std::vector<Node<K, V>*>& predecessors) {
        Node<K, V>* current = header_;

        for (int i = current_max_level_; i >= 0; i--) {
            current = move_forward_in_level(current, i, key);
            predecessors[i] = current;
        }
    }

    Node<K, V>* move_forward_in_level(Node<K, V>* current, int level,
                                      const K& key) {
        while (current->forward_[level] != nullptr &&
               current->forward_[level]->key_ < key) {
            current = current->forward_[level];
        }
        return current;
    }

    Node<K, V>* get_target_node(Node<K, V>* predecessor, const K& key) {
        Node<K, V>* target = predecessor->forward_[0];
        if (target != nullptr && target->key_ == key) {
            return target;
        }
        return nullptr;
    }

    Node<K, V>* get_node_at_level_zero(Node<K, V>* predecessor, const K& key) {
        Node<K, V>* node = predecessor->forward_[0];
        if (node != nullptr && node->key_ == key) {
            return node;
        }
        return nullptr;
    }

    void update_existing_node(Node<K, V>* node, const V& value) {
        node->value_ = value;
    }

    void insert_new_node(const K& key, const V& value,
                         const std::vector<Node<K, V>*>& predecessors) {
        uint8_t new_level = generate_random_level();
        adjust_max_level_for_insertion(
            new_level, const_cast<std::vector<Node<K, V>*>&>(predecessors));

        Node<K, V>* new_node = create_node(key, value, new_level);
        insert_node_at_all_levels(new_node, predecessors, new_level);

        element_count_++;
    }

    uint8_t generate_random_level() {
        uint8_t level = 0;
        while (distribution_(gen_) && level < max_level_) {
            level++;
        }
        return level;
    }

    void adjust_max_level_for_insertion(
        uint8_t new_level, std::vector<Node<K, V>*>& predecessors) {
        if (new_level > current_max_level_) {
            for (uint8_t i = current_max_level_ + 1; i <= new_level; i++) {
                predecessors[i] = header_;
            }
            current_max_level_ = new_level;
        }
    }

    Node<K, V>* create_node(const K& key, const V& value, uint8_t level) {
        return new Node<K, V>(key, value, level);
    }

    void insert_node_at_all_levels(Node<K, V>* node,
                                   const std::vector<Node<K, V>*>& predecessors,
                                   uint8_t level) {
        for (uint8_t i = 0; i <= level; i++) {
            node->forward_[i] = predecessors[i]->forward_[i];
            predecessors[i]->forward_[i] = node;
        }
    }

    void delete_node(Node<K, V>* node,
                     const std::vector<Node<K, V>*>& predecessors) {
        disconnect_node_from_levels(node, predecessors);
        delete node;
        element_count_--;
    }

    void disconnect_node_from_levels(
        Node<K, V>* node, const std::vector<Node<K, V>*>& predecessors) {
        for (uint8_t i = 0; i <= current_max_level_; i++) {
            if (predecessors[i]->forward_[i] == node) {
                predecessors[i]->forward_[i] = node->forward_[i];
            }
        }
    }

    void adjust_max_level() {
        while (current_max_level_ > 0 &&
               header_->forward_[current_max_level_] == nullptr) {
            current_max_level_--;
        }
    }

    void clear() {
        delete_chain_starting_from(header_->forward_[0]);
        delete header_;
    }

    void delete_chain_starting_from(Node<K, V>* node) {
        if (node == nullptr) {
            return;
        }

        Node<K, V>* next = node->forward_[0];
        delete node;
        delete_chain_starting_from(next);
    }

    uint8_t max_level_;
    uint8_t current_max_level_{0};
    Node<K, V>* header_{nullptr};
    size_t element_count_{0};

    mutable std::mutex mutex_;
    std::mt19937 gen_;
    std::bernoulli_distribution distribution_;
};

}  // namespace skip_list
}  // namespace momu

#endif  // MOMU_SKIP_LIST_H