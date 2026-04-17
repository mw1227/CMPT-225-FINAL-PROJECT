#pragma once
#include "hash_common.h"
#include <vector>

class LinearProbingHashTable : public HashTableBase {
public:
    explicit LinearProbingHashTable(size_t initial_capacity = 1024)
        : slots_(initial_capacity), size_(0), tombstones_(0), cap_(initial_capacity) {}

    void insert(uint64_t key, uint64_t value) override {
        // Resize if load (including tombstones) is too high
        if ((size_ + tombstones_) * 100 / cap_ > 70) {
            resize(cap_ * 2);
        }

        size_t idx = hash_(key, cap_);
        int first_tombstone = -1;

        for (size_t i = 0; i < cap_; i++) {
            size_t pos = (idx + i) & (cap_ - 1);  // power-of-2 mod

            if (slots_[pos].state == SlotState::EMPTY) {
                size_t insert_pos = (first_tombstone >= 0) ? first_tombstone : pos;
                slots_[insert_pos] = {key, value, SlotState::OCCUPIED};
                size_++;
                if (first_tombstone >= 0) tombstones_--;
                return;
            }
            if (slots_[pos].state == SlotState::DELETED) {
                if (first_tombstone < 0) first_tombstone = pos;
                return;
            }
            if (slots_[pos].key == key) {
                slots_[pos].value = value;  // update existing
                return;
            }
        }
    }

    std::optional<uint64_t> lookup(uint64_t key) const override {
        size_t idx = hash_(key, cap_);

        for (size_t i = 0; i < cap_; i++) {
            size_t pos = (idx + i) & (cap_ - 1);

            if (slots_[pos].state == SlotState::EMPTY)
                return std::nullopt;

            if (slots_[pos].state == SlotState::OCCUPIED &&
                slots_[pos].key == key)
                return slots_[pos].value;

            // Deleted: continue probing
        }
        return std::nullopt;
    }

    bool remove(uint64_t key) override {
        size_t idx = hash_(key, cap_);

        for (size_t i = 0; i < cap_; i++) {
            size_t pos = (idx + i) & (cap_ - 1);

            if (slots_[pos].state == SlotState::EMPTY)
                return false;

            if (slots_[pos].state == SlotState::OCCUPIED &&
                slots_[pos].key == key) {
                slots_[pos].state = SlotState::DELETED;
                size_--;
                tombstones_++;

                // Rehash if tombstone density is too high
                if (tombstones_ * 4 > cap_) {
                    resize(cap_);  // same size, just clears tombstones
                }
                return true;
            }
        }
        return false;
    }

    size_t size()     const override { return size_; }
    size_t capacity() const override { return cap_;  }
    std::string name() const override { return "Linear Probing"; }

    size_t bytes_used() const override {
        return cap_ * sizeof(Slot);
    }

private:
    struct Slot {
        uint64_t  key   = 0;
        uint64_t  value = 0;
        SlotState state = SlotState::EMPTY;
    };

    std::vector<Slot> slots_;
    size_t size_;
    size_t tombstones_;
    size_t cap_;
    FibonacciHash hash_;

    void resize(size_t new_cap) {
        std::vector<Slot> old_slots = std::move(slots_);
        size_t old_cap = cap_;

        slots_.assign(new_cap, Slot{});
        cap_ = new_cap;
        size_ = 0;
        tombstones_ = 0;

        for (size_t i = 0; i < old_cap; i++) {
            if (old_slots[i].state == SlotState::OCCUPIED) {
                insert(old_slots[i].key, old_slots[i].value);
            }
        }
    }
};
