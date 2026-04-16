#pragma once
#include "hash_common.h"
#include <vector>
#include <random>

class CuckooHashTable : public HashTableBase {
public:
    explicit CuckooHashTable(size_t initial_capacity = 1024)
        : table1_(initial_capacity / 2),
          table2_(initial_capacity / 2),
          size_(0),
          cap_(initial_capacity / 2),
          seed1_(0x12345678),
          seed2_(0xDEADBEEF),
          rng_(42) {}

    void insert(uint64_t key, uint64_t value) override {
        // Check if already present, update if so
        size_t pos1 = h1(key);
        if (table1_[pos1].state == SlotState::OCCUPIED &&
            table1_[pos1].key == key) {
            table1_[pos1].value = value;
            return;
        }
        size_t pos2 = h2(key);
        if (table2_[pos2].state == SlotState::OCCUPIED &&
            table2_[pos2].key == key) {
            table2_[pos2].value = value;
            return;
        }

        // Resize if load factor is too high (per-table > 0.45)
        if (size_ * 100 / (cap_ * 2) > 45) {
            resize(cap_ * 2);
        }

        insert_impl(key, value);
    }

    std::optional<uint64_t> lookup(uint64_t key) const override {
        size_t pos1 = h1(key);
        if (table1_[pos1].state == SlotState::OCCUPIED &&
            table1_[pos1].key == key) {
            return table1_[pos1].value;
        }

        size_t pos2 = h2(key);
        if (table2_[pos2].state == SlotState::OCCUPIED &&
            table2_[pos2].key == key) {
            return table2_[pos2].value;
        }

        return std::nullopt;
    }

    bool remove(uint64_t key) override {
        size_t pos1 = h1(key);
        if (table1_[pos1].state == SlotState::OCCUPIED &&
            table1_[pos1].key == key) {
            table1_[pos1].state = SlotState::EMPTY;
            size_--;
            return true;
        }

        size_t pos2 = h2(key);
        if (table2_[pos2].state == SlotState::OCCUPIED &&
            table2_[pos2].key == key) {
            table2_[pos2].state = SlotState::EMPTY;
            size_--;
            return true;
        }

        return false;
    }

    size_t size()     const override { return size_;      }
    size_t capacity() const override { return cap_ * 2;   }
    std::string name() const override { return "Cuckoo"; }

    size_t bytes_used() const override {
        return cap_ * 2 * sizeof(Slot);
    }

private:
    static constexpr int MAX_KICKS = 500;

    struct Slot {
        uint64_t  key   = 0;
        uint64_t  value = 0;
        SlotState state = SlotState::EMPTY;
    };

    std::vector<Slot> table1_;
    std::vector<Slot> table2_;
    size_t size_;
    size_t cap_;       // capacity of each individual table
    uint64_t seed1_;
    uint64_t seed2_;
    std::mt19937_64 rng_;

    // Hash functions with seeds for rehashing with new functions
    size_t h1(uint64_t key) const {
        uint64_t h = (key ^ seed1_) * 11400714819323198485ULL;
        h ^= h >> 32;
        return h & (cap_ - 1);
    }

    size_t h2(uint64_t key) const {
        uint64_t h = (key ^ seed2_) * 14029467366897019727ULL;
        h ^= h >> 32;
        return h & (cap_ - 1);
    }

    void insert_impl(uint64_t key, uint64_t value) {
        uint64_t cur_key = key;
        uint64_t cur_val = value;

        for (int kick = 0; kick < MAX_KICKS; kick++) {
            // Try table 1
            size_t pos1 = h1(cur_key);
            if (table1_[pos1].state != SlotState::OCCUPIED) {
                table1_[pos1] = {cur_key, cur_val, SlotState::OCCUPIED};
                size_++;
                return;
            }
            // Evict from table 1
            std::swap(cur_key, table1_[pos1].key);
            std::swap(cur_val, table1_[pos1].value);

            // Try table 2
            size_t pos2 = h2(cur_key);
            if (table2_[pos2].state != SlotState::OCCUPIED) {
                table2_[pos2] = {cur_key, cur_val, SlotState::OCCUPIED};
                size_++;
                return;
            }
            // Evict from table 2
            std::swap(cur_key, table2_[pos2].key);
            std::swap(cur_val, table2_[pos2].value);
        }

        // Cycle detected. Rehash with new seeds and retry
        rehash();
        insert_impl(cur_key, cur_val);
    }

    void rehash() {
        // Pick new random seeds
        seed1_ = rng_();
        seed2_ = rng_();

        std::vector<Slot> old_t1 = std::move(table1_);
        std::vector<Slot> old_t2 = std::move(table2_);
        size_t old_cap = cap_;

        table1_.assign(cap_, Slot{});
        table2_.assign(cap_, Slot{});
        size_ = 0;

        for (size_t i = 0; i < old_cap; i++) {
            if (old_t1[i].state == SlotState::OCCUPIED)
                insert_impl(old_t1[i].key, old_t1[i].value);
            if (old_t2[i].state == SlotState::OCCUPIED)
                insert_impl(old_t2[i].key, old_t2[i].value);
        }
    }

    void resize(size_t new_per_table_cap) {
        seed1_ = rng_();
        seed2_ = rng_();

        std::vector<Slot> old_t1 = std::move(table1_);
        std::vector<Slot> old_t2 = std::move(table2_);
        size_t old_cap = cap_;

        cap_ = new_per_table_cap;
        table1_.assign(cap_, Slot{});
        table2_.assign(cap_, Slot{});
        size_ = 0;

        for (size_t i = 0; i < old_cap; i++) {
            if (old_t1[i].state == SlotState::OCCUPIED)
                insert_impl(old_t1[i].key, old_t1[i].value);
            if (old_t2[i].state == SlotState::OCCUPIED)
                insert_impl(old_t2[i].key, old_t2[i].value);
        }
    }
};
