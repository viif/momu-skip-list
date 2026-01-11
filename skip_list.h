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
    Node() = default;
    Node(const K& key, const V& value, uint8_t level)
        : key_(key), value_(value), forward_(level + 1) {}

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    K key_;
    V value_;
    std::vector<std::shared_ptr<Node<K, V>>> forward_;
};

template <typename K, typename V>
class SkipList {
   public:
    explicit SkipList(uint8_t max_level,
                      unsigned int seed = std::random_device{}())
        : max_level_(max_level),
          header_(std::make_unique<Node<K, V>>(K{}, V{}, max_level_)),
          gen_(seed),
          distribution_(0.5) {}

    SkipList(const SkipList&) = delete;
    SkipList& operator=(const SkipList&) = delete;

    void put(const K& key, const V& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto predecessors = find_predecessors(key);
        if (auto* exist = get_node_at_level_zero(predecessors[0], key)) {
            exist->value_ = value;
        } else {
            insert_new_node(key, value, predecessors);
        }
    }

    std::optional<V> get(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (auto* node = find_node(key)) return node->value_;
        return std::nullopt;
    }

    bool contains(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return find_node(key) != nullptr;
    }

    bool remove(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto predecessors = find_predecessors(key);
        auto* victim = get_node_at_level_zero(predecessors[0], key);
        if (!victim) return false;

        delete_node(victim, predecessors);
        adjust_max_level();
        return true;
    }

    size_t size() const { return element_count_; }
    bool empty() const { return element_count_ == 0; }

   private:
    Node<K, V>* find_node(const K& key) { return traverse_to_level_zero(key); }

    using PredVec = std::vector<Node<K, V>*>;
    PredVec find_predecessors(const K& key) {
        PredVec preds(max_level_ + 1, nullptr);
        traverse_and_collect_predecessors(key, preds);
        return preds;
    }

    Node<K, V>* traverse_to_level_zero(const K& key) {
        Node<K, V>* cur = header_.get();
        for (int i = current_max_level_; i >= 0; --i)
            cur = move_forward_in_level(cur, i, key);
        return get_target_node(cur, key);
    }

    void traverse_and_collect_predecessors(const K& key, PredVec& preds) {
        Node<K, V>* cur = header_.get();
        for (int i = current_max_level_; i >= 0; --i) {
            cur = move_forward_in_level(cur, i, key);
            preds[i] = cur;
        }
    }

    Node<K, V>* move_forward_in_level(Node<K, V>* cur, int lvl, const K& key) {
        while (cur->forward_[lvl] && cur->forward_[lvl]->key_ < key)
            cur = cur->forward_[lvl].get();
        return cur;
    }

    Node<K, V>* get_target_node(Node<K, V>* pred, const K& key) {
        auto* nxt = pred->forward_[0].get();
        return (nxt && nxt->key_ == key) ? nxt : nullptr;
    }

    Node<K, V>* get_node_at_level_zero(Node<K, V>* pred, const K& key) {
        auto* nxt = pred->forward_[0].get();
        return (nxt && nxt->key_ == key) ? nxt : nullptr;
    }

    void insert_new_node(const K& key, const V& value, const PredVec& preds) {
        uint8_t lvl = generate_random_level();
        PredVec mutable_preds = preds;
        adjust_max_level_for_insertion(lvl, mutable_preds);

        auto new_node = std::make_shared<Node<K, V>>(key, value, lvl);
        for (uint8_t i = 0; i <= lvl; ++i) {
            new_node->forward_[i] = mutable_preds[i]->forward_[i];
            mutable_preds[i]->forward_[i] = new_node;
        }
        ++element_count_;
    }

    void delete_node(Node<K, V>* node, const PredVec& preds) {
        for (uint8_t i = 0; i <= current_max_level_; ++i) {
            if (preds[i]->forward_[i].get() == node)
                preds[i]->forward_[i] = node->forward_[i];
        }
        --element_count_;
    }

    void adjust_max_level() {
        while (current_max_level_ > 0 && !header_->forward_[current_max_level_])
            --current_max_level_;
    }

    uint8_t generate_random_level() {
        uint8_t lvl = 0;
        while (get_half_probability() && lvl < max_level_) {
            ++lvl;
        }
        return lvl;
    }

    bool get_half_probability() { return distribution_(gen_); }

    void adjust_max_level_for_insertion(uint8_t lvl, PredVec& preds) {
        if (lvl > current_max_level_) {
            for (uint8_t i = current_max_level_ + 1; i <= lvl; ++i)
                preds[i] = header_.get();
            current_max_level_ = lvl;
        }
    }

    uint8_t max_level_;
    uint8_t current_max_level_{0};
    std::unique_ptr<Node<K, V>> header_;
    size_t element_count_{0};

    mutable std::mutex mutex_;
    std::mt19937 gen_;
    std::bernoulli_distribution distribution_;
};

}  // namespace skip_list
}  // namespace momu

#endif  // MOMU_SKIP_LIST_H