#ifndef SIMPLE_HASHING_H
#define SIMPLE_HASHING_H

#include <cstdint>
#include <vector>
#include <cstring>
#include <cassert>
#include <NTL/ZZ.h>
#include <NTL/lzz_p.h>

using namespace NTL;

class SimpleHashing {
public:
    SimpleHashing(uint64_t num_bins, uint64_t num_hash_funcs)
        : b(num_bins), gamma(num_hash_funcs)
    {
        table.resize(b);
    }

    uint64_t hash_to_bin(const zz_p& item, uint64_t func_idx) const
    {
        uint64_t item_val = static_cast<uint64_t>(conv<unsigned long>(item));
        uint64_t h = (item_val * (func_idx + 1) * 0x9e3779b97f4a7c15ULL) >> (64 - log2_ceil(b));
        return h % b;
    }

    void insert(const zz_p& item)
    {
        for (uint64_t j = 0; j < gamma; j++) {
            uint64_t bin = hash_to_bin(item, j);
            table[bin].push_back(item);
        }
    }

    void pad_empty_bins(const zz_p& dummy_item)
    {
        for (uint64_t i = 0; i < b; i++) {
            if (table[i].empty()) {
                table[i].push_back(dummy_item);
            }
        }
    }

    uint64_t num_bins() const { return b; }
    uint64_t num_hash_funcs() const { return gamma; }

    const std::vector<zz_p>& get_bin(uint64_t idx) const { return table[idx]; }
    uint64_t bin_size(uint64_t idx) const { return table[idx].size(); }

    std::vector<std::vector<zz_p>> table;
    uint64_t b;
    uint64_t gamma;

private:
    uint64_t log2_ceil(uint64_t n) const
    {
        uint64_t r = 0;
        while ((1ULL << r) < n) r++;
        return r;
    }
};

#endif
