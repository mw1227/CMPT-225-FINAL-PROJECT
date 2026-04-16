#pragma once
#include "hash_common.h"
#include <vector>

class RobinHoodHashTable : public HashTableBase {
public:
    explicit RobinHoodHashTable(size_t initial_capacity = 1024)
        : slots_(initial_capacity), size_(0), cap_(initial_capacity) {}

    void insert(uint64_t key, uint64_t value) override {
        if (size_ * 100 / cap_ > 80) {
            resize(cap_ * 2);
        }

        uint64_t ins_key = key;
        uint64_t ins_val = value;
        size_t   pos     = hash_(ins_key, cap_);
        size_t   dist    = 0;

        for (size_t i = 0; i < cap_; i++) {
            // place if empty
            if (slots_[pos].state != SlotState::OCCUPIED) {
                slots_[pos] = {ins_key, ins_val, SlotState::OCCUPIED};
                size_++;
                return;
            }

            // update if exists
            if (slots_[pos].key == ins_key) {
                slots_[pos].value = ins_val;
                return;
            }

            // occupant, swap and continue inserting the displaced element.
            size_t existing_dist = probe_distance(pos);
            if (dist > existing_dist) {
                std::swap(ins_key, slots_[pos].key);
                std::swap(ins_val, slots_[pos].value);
                dist = existing_dist;
            }
            pos = (pos + 1) & (cap_ - 1);
            dist++;
        }
    }

    std::optional<uint64_t> lookup(uint64_t key) const override {
        size_t idx  = hash_(key, cap_);
        size_t dist = 0;

        for (size_t i = 0; i < cap_; i++) {
            size_t pos = (idx + dist) & (cap_ - 1);

            if (slots_[pos].state != SlotState::OCCUPIED)
                return std::nullopt;

            // less than ours, the key cannot be present further along.
            if (probe_distance(pos) < dist)
                return std::nullopt;

            if (slots_[pos].key == key)
                return slots_[pos].value;

            dist++;
        }
        return std::nullopt;
    }

    bool remove(uint64_t key) override {
        size_t idx  = hash_(key, cap_);
        size_t dist = 0;

        // Find the key
        for (size_t i = 0; i < cap_; i++) {
            size_t pos = (idx + dist) & (cap_ - 1);

            if (slots_[pos].state != SlotState::OCCUPIED)
                return false;

            if (probe_distance(pos) < dist)
                return false;

            if (slots_[pos].key == key) {
                // to fill the gap, maintaining the Robin Hood invariant.
                size_t hole = pos;
                for (size_t j = 1; j < cap_; j++) {
                    size_t next = (pos + j) & (cap_ - 1);

                    if (slots_[next].state != SlotState::OCCUPIED ||
                        probe_distance(next) == 0) {
                        // stop if next slot is empty or correct
                        break;
                    }

                    // Shift element backward
                    slots_[hole] = slots_[next];
                    hole = next;
                }
                slots_[hole].state = SlotState::EMPTY;
                size_--;
                return true;
            }
            dist++;
        }
        return false;
    }

    size_t size()     const override { return size_; }
    size_t capacity() const override { return cap_;  }
    std::string name() const override { return "Robin Hood"; }

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
    size_t cap_;
    FibonacciHash hash_;

    // Compute how far a slot's occupant is from its ideal position.
    size_t probe_distance(size_t pos) const {
        size_t ideal = hash_(slots_[pos].key, cap_);
        return (pos - ideal + cap_) & (cap_ - 1);
    }

    void resize(size_t new_cap) {
        std::vector<Slot> old_slots = std::move(slots_);
        size_t old_cap = cap_;

        slots_.assign(new_cap, Slot{});
        cap_ = new_cap;
        size_ = 0;

        for (size_t i = 0; i < old_cap; i++) {
            if (old_slots[i].state == SlotState::OCCUPIED) {
                insert(old_slots[i].key, old_slots[i].value);
            }
        }
    }
};
