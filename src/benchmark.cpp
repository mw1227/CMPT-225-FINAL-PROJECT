#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>
#include <iomanip>
#include <memory>
#include <functional>
#include <numeric>
#include "hash_common.h"
#include "chaining.h"
#include "linear_probing.h"
#include "robin_hood.h"
#include "cuckoo.h"

// Timing utility
using Clock = std::chrono::high_resolution_clock;

template<typename Func>
double measure_mops(Func&& fn, size_t num_ops) {
    auto start = Clock::now();
    fn();
    auto end = Clock::now();
    double seconds = std::chrono::duration<double>(end - start).count();
    return (num_ops / seconds) / 1e6;  // millions of ops per second
}

// Key generators
std::vector<uint64_t> generate_keys(size_t count, uint64_t seed = 42) {
    std::mt19937_64 rng(seed);
    std::vector<uint64_t> keys(count);
    for (auto& k : keys) k = rng();
    return keys;
}

std::vector<uint64_t> generate_absent_keys(size_t count,
                                            const std::vector<uint64_t>& present,
                                            uint64_t seed = 999) {
    std::mt19937_64 rng(seed);
    std::vector<uint64_t> absent;
    absent.reserve(count);
    for (size_t i = 0; i < count; i++) {
        absent.push_back(rng() | (1ULL << 63));  // set high bit to avoid overlap
    }
    return absent;
}

// Single benchmark run
struct BenchResult {
    std::string variant;
    double      load_factor;
    double      insert_mops;
    double      lookup_succ_mops;
    double      lookup_fail_mops;
    double      delete_mops;
    size_t      bytes_per_element;
};

using TableFactory = std::function<std::unique_ptr<HashTableBase>(size_t)>;

BenchResult run_benchmark(TableFactory factory, const std::string& name,
                          double target_alpha, size_t table_size,
                          int num_trials) {

    size_t num_elements = static_cast<size_t>(table_size * target_alpha);
    if (num_elements < 100) num_elements = 100;

    size_t num_ops = std::min(num_elements, (size_t)500000);

    std::vector<double> insert_results, lookup_succ_results,
                        lookup_fail_results, delete_results;

    for (int trial = 0; trial < num_trials; trial++) {
        auto keys    = generate_keys(num_elements, 42 + trial * 1000);
        auto absent  = generate_absent_keys(num_ops, keys, 9999 + trial * 1000);

        // Insert benchmark
        {
            auto table = factory(table_size);
            double mops = measure_mops([&]() {
                for (size_t i = 0; i < num_elements; i++) {
                    table->insert(keys[i], keys[i] * 2);
                }
            }, num_elements);
            insert_results.push_back(mops);
        }

        // Lookup successful benchmark
        {
            auto table = factory(table_size);
            for (size_t i = 0; i < num_elements; i++)
                table->insert(keys[i], keys[i] * 2);

            std::vector<uint64_t> lookup_keys(keys.begin(),
                                               keys.begin() + num_ops);
            std::mt19937_64 shuf_rng(trial);
            std::shuffle(lookup_keys.begin(), lookup_keys.end(), shuf_rng);

            double mops = measure_mops([&]() {
                volatile uint64_t sink = 0;
                for (auto& k : lookup_keys) {
                    auto val = table->lookup(k);
                    if (val) sink = *val;
                }
            }, num_ops);
            lookup_succ_results.push_back(mops);
        }

        // Lookup unsuccessful benchmark
        {
            auto table = factory(table_size);
            for (size_t i = 0; i < num_elements; i++)
                table->insert(keys[i], keys[i] * 2);

            double mops = measure_mops([&]() {
                volatile uint64_t sink = 0;
                for (size_t i = 0; i < num_ops; i++) {
                    auto val = table->lookup(absent[i]);
                    if (val) sink = *val;
                }
            }, num_ops);
            lookup_fail_results.push_back(mops);
        }

        // Delete benchmark
        {
            auto table = factory(table_size);
            for (size_t i = 0; i < num_elements; i++)
                table->insert(keys[i], keys[i] * 2);

            std::vector<uint64_t> del_keys(keys.begin(),
                                            keys.begin() + num_ops);
            std::mt19937_64 del_rng(trial + 777);
            std::shuffle(del_keys.begin(), del_keys.end(), del_rng);

            double mops = measure_mops([&]() {
                for (auto& k : del_keys) {
                    table->remove(k);
                }
            }, num_ops);
            delete_results.push_back(mops);
        }
    }

    auto median = [](std::vector<double>& v) -> double {
        std::sort(v.begin(), v.end());
        return v[v.size() / 2];
    };

    auto table = factory(table_size);
    auto keys = generate_keys(num_elements);
    for (size_t i = 0; i < num_elements; i++)
        table->insert(keys[i], keys[i] * 2);

    size_t bpe = (table->size() > 0) ?
                  table->bytes_used() / table->size() : 0;

    return {
        name,
        target_alpha,
        median(insert_results),
        median(lookup_succ_results),
        median(lookup_fail_results),
        median(delete_results),
        bpe
    };
}

// print latex coords to csv
void print_csv(const std::vector<BenchResult>& results) {
    std::cout << "\n";
    std::cout << "  CSV RESULTS\n";
    std::cout << "variant,load_factor,insert_mops,lookup_succ_mops,"
              << "lookup_fail_mops,delete_mops,bytes_per_element\n";

    for (auto& r : results) {
        std::cout << r.variant << ","
                  << std::fixed << std::setprecision(2) << r.load_factor << ","
                  << std::setprecision(2) << r.insert_mops << ","
                  << r.lookup_succ_mops << ","
                  << r.lookup_fail_mops << ","
                  << r.delete_mops << ","
                  << r.bytes_per_element << "\n";
    }
}

// print latex coords to stdout
void print_latex_coords(const std::vector<BenchResult>& results) {
    std::cout << "\n";
    std::cout << "  LATEX COORDINATES\n";

    std::vector<std::string> variants = {
        "Separate Chaining", "Linear Probing", "Robin Hood", "Cuckoo"
    };
    std::vector<std::string> op_labels = {
        "Insert", "Successful Lookup", "Unsuccessful Lookup", "Delete"
    };

    for (size_t op = 0; op < 4; op++) {
        std::cout << "\n% ── " << op_labels[op]
                  << " throughput (Figure " << (op + 1) << ")\n\n";

        for (auto& var : variants) {
            std::cout << "% " << var << "\n";
            std::cout << "\\addplot coordinates {\n    ";

            bool first = true;
            for (auto& r : results) {
                if (r.variant != var) continue;
                if (!first) std::cout << " ";
                double val = 0;
                if      (op == 0) val = r.insert_mops;
                else if (op == 1) val = r.lookup_succ_mops;
                else if (op == 2) val = r.lookup_fail_mops;
                else              val = r.delete_mops;

                std::cout << "(" << std::fixed << std::setprecision(2)
                          << r.load_factor << ","
                          << std::setprecision(1) << val << ")";
                first = false;
            }
            std::cout << "\n};\n\n";
        }
    }

    std::cout << "\n%Memory usage (Table 1)\n";
    std::cout << "% variant | bytes_per_element (at alpha=0.5)\n";
    for (auto& r : results) {
        if (std::abs(r.load_factor - 0.5) < 0.05 ||
            (r.variant == "Cuckoo" && std::abs(r.load_factor - 0.4) < 0.05)) {
            std::cout << "% " << r.variant << ": "
                      << r.bytes_per_element << " bytes/element\n";
        }
    }
}

// Correctness smoke test
bool smoke_test(TableFactory factory, const std::string& name) {
    auto table = factory(64);

    for (uint64_t i = 1; i <= 20; i++) {
        table->insert(i, i * 100);
    }
    if (table->size() != 20) {
        std::cerr << name << ": size mismatch after insert\n";
        return false;
    }

    for (uint64_t i = 1; i <= 20; i++) {
        auto val = table->lookup(i);
        if (!val || *val != i * 100) {
            std::cerr << name << ": lookup failed for key " << i << "\n";
            return false;
        }
    }

    for (uint64_t i = 100; i <= 110; i++) {
        auto val = table->lookup(i);
        if (val) {
            std::cerr << name << ": false positive for key " << i << "\n";
            return false;
        }
    }

    table->insert(5, 999);
    auto val = table->lookup(5);
    if (!val || *val != 999) {
        std::cerr << name << ": update failed\n";
        return false;
    }

    if (!table->remove(5)) {
        std::cerr << name << ": delete returned false\n";
        return false;
    }
    if (table->lookup(5)) {
        std::cerr << name << ": key still present after delete\n";
        return false;
    }
    if (table->size() != 19) {
        std::cerr << name << ": size mismatch after delete\n";
        return false;
    }

    std::cout << "  [PASS] " << name << "\n";
    return true;
}

int main() {
    auto make_chaining = [](size_t cap) -> std::unique_ptr<HashTableBase> {
        return std::make_unique<ChainingHashTable>(cap);
    };
    auto make_linear = [](size_t cap) -> std::unique_ptr<HashTableBase> {
        return std::make_unique<LinearProbingHashTable>(cap);
    };
    auto make_robin = [](size_t cap) -> std::unique_ptr<HashTableBase> {
        return std::make_unique<RobinHoodHashTable>(cap);
    };
    auto make_cuckoo = [](size_t cap) -> std::unique_ptr<HashTableBase> {
        return std::make_unique<CuckooHashTable>(cap);
    };

    // Correctness tests
    std::cout << "Running correctness tests\n";
    bool all_pass = true;
    all_pass &= smoke_test(make_chaining, "Separate Chaining");
    all_pass &= smoke_test(make_linear,   "Linear Probing");
    all_pass &= smoke_test(make_robin,    "Robin Hood");
    all_pass &= smoke_test(make_cuckoo,   "Cuckoo");

    if (!all_pass) {
        std::cerr << "\nSome tests failed. Fix bugs before benchmarking.\n";
        return 1;
    }
    std::cout << "All correctness tests passed.\n\n";

    // Benchmark parameters
    const size_t TABLE_SIZE = 1 << 20;  // 1,048,576 slots
    const int    NUM_TRIALS = 5;

    std::vector<double> alphas = {0.1, 0.2, 0.3, 0.4, 0.5,
                                  0.6, 0.7, 0.8, 0.9, 0.95};
    std::vector<double> cuckoo_alphas = {0.1, 0.2, 0.3, 0.4, 0.45};

    struct VariantConfig {
        std::string  name;
        TableFactory factory;
        std::vector<double> alphas;
    };

    std::vector<VariantConfig> configs = {
        {"Separate Chaining", make_chaining, alphas},
        {"Linear Probing",    make_linear,   alphas},
        {"Robin Hood",        make_robin,    alphas},
        {"Cuckoo",            make_cuckoo,   cuckoo_alphas},
    };

    // Run benchmarks
    std::vector<BenchResult> all_results;

    for (auto& cfg : configs) {
        std::cout << "Benchmarking: " << cfg.name << "\n";

        for (double alpha : cfg.alphas) {
            std::cout << "  alpha=" << std::fixed << std::setprecision(2)
                      << alpha << " ... " << std::flush;

            auto result = run_benchmark(cfg.factory, cfg.name,
                                        alpha, TABLE_SIZE, NUM_TRIALS);

            std::cout << "insert=" << std::setprecision(1) << result.insert_mops
                      << " succ=" << result.lookup_succ_mops
                      << " fail=" << result.lookup_fail_mops
                      << " del="  << result.delete_mops
                      << " Mops/s\n";

            all_results.push_back(result);
        }
        std::cout << "\n";
    }

    // Print results to terminal
    print_csv(all_results);
    print_latex_coords(all_results);

    std::cout << "\nDone.\n";
    return 0;
}