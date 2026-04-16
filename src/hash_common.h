#pragma once
#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>

// Slot states for open-addressing variants
enum class SlotState : uint8_t {
    EMPTY    = 0,
    OCCUPIED = 1,
    DELETED  = 2   // tombstone (used by linear probing)
};

// Fibonacci hashing
struct FibonacciHash {
    static constexpr uint64_t PHI_INV = 11400714819323198485ULL; // 2^64 / phi

    size_t operator()(uint64_t key, size_t capacity) const {
        return static_cast<size_t>((key * PHI_INV) >> (64 - __builtin_ctzll(capacity)));
    }
};

// Secondary hash for cuckoo hashing
// A different multiplicative hash to provide an independent probe location.
struct SecondaryHash {
    static constexpr uint64_t MULT = 14029467366897019727ULL;

    size_t operator()(uint64_t key, size_t capacity) const {
        uint64_t h = key * MULT;
        h ^= (h >> 33);
        h *= 0xff51afd7ed558ccdULL;
        return static_cast<size_t>((h) >> (64 - __builtin_ctzll(capacity)));
    }
};

// Abstract hash table interface
// All four variants implement this interface for uniform benchmarking.
class HashTableBase {
public:
    virtual ~HashTableBase() = default;

    virtual void insert(uint64_t key, uint64_t value) = 0;
    virtual std::optional<uint64_t> lookup(uint64_t key) const = 0;
    virtual bool remove(uint64_t key) = 0;

    virtual size_t size() const = 0;
    virtual size_t capacity() const = 0;
    virtual std::string name() const = 0;
    virtual size_t bytes_used() const = 0;

    double load_factor() const {
        return static_cast<double>(size()) / capacity();
    }
};
