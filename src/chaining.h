#pragma once
#include "hash_common.h"
#include <vector>
#include <memory>

class ChainingHashTable : public HashTableBase {
public:
    explicit ChainingHashTable(size_t initial_capacity = 1024)
        : buckets_(initial_capacity, nullptr), size_(0), cap_(initial_capacity) {}

    ~ChainingHashTable() override {
        for (auto& head : buckets_) {
            Node* curr = head;
            while (curr) {
                Node* next = curr->next;
                delete curr;
                curr = next;
            }
        }
    }

    // dont allow copys
    ChainingHashTable(const ChainingHashTable&) = delete;
    ChainingHashTable& operator=(const ChainingHashTable&) = delete;

    void insert(uint64_t key, uint64_t value) override {
        if (size_ >= cap_) resize(cap_ * 2);

        size_t idx = hash_(key, cap_);

        // Update if key already exists
        for (Node* n = buckets_[idx]; n; n = n->next) {
            if (n->key == key) {
                n->value = value;
                return;
            }
        }

        // Prepend new node
        buckets_[idx] = new Node{key, value, buckets_[idx]};
        size_++;
    }

    std::optional<uint64_t> lookup(uint64_t key) const override {
        size_t idx = hash_(key, cap_);
        for (Node* n = buckets_[idx]; n; n = n->next) {
            if (n->key == key) return n->value;
        }
        return std::nullopt;
    }

    bool remove(uint64_t key) override {
        size_t idx = hash_(key, cap_);
        Node* prev = nullptr;
        Node* curr = buckets_[idx];

        while (curr) {
            if (curr->key == key) {
                if (prev) prev->next = curr->next;
                else      buckets_[idx] = curr->next;
                delete curr;
                size_--;
                return true;
            }
            prev = curr;
            curr = curr->next;
        }
        return false;
    }

    size_t size()     const override { return size_; }
    size_t capacity() const override { return cap_;  }
    std::string name() const override { return "Separate Chaining"; }

    size_t bytes_used() const override {
        // Array of pointers + per-node overhead (key + value + pointer)
        return cap_ * sizeof(Node*) + size_ * sizeof(Node);
    }

private:
    struct Node {
        uint64_t key;
        uint64_t value;
        Node*    next;
    };

    std::vector<Node*> buckets_;
    size_t size_;
    size_t cap_;
    FibonacciHash hash_;

    void resize(size_t new_cap) {
        std::vector<Node*> old_buckets(new_cap, nullptr);
        std::swap(old_buckets, buckets_);
        cap_ = new_cap;
        size_ = 0;

        for (auto& head : old_buckets) {
            Node* curr = head;
            while (curr) {
                Node* next = curr->next;
                // Re-insert into new table
                size_t idx = hash_(curr->key, cap_);
                curr->next = buckets_[idx];
                buckets_[idx] = curr;
                size_++;
                curr = next;
            }
        }
    }
};
