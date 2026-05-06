#ifndef CUCKOO_HASHING_H
#define CUCKOO_HASHING_H

#include <cstdint>
#include <vector>
#include <cstring>
#include <cassert>
#include <NTL/ZZ.h>
#include <NTL/lzz_p.h>

using namespace NTL;

struct CuckooHashEntry {
    zz_p item;
    bool is_dummy;
};

class CuckooHashing {
public:
    CuckooHashing(uint64_t num_bins, uint64_t num_hash_funcs, uint64_t stash_size = 0)
        : b(num_bins), gamma(num_hash_funcs), stash_sz(stash_size)
    {
        table.resize(b);
        for (auto& entry : table) {
            entry.is_dummy = true;
        }
        stash.resize(stash_sz);
        for (auto& entry : stash) {
            entry.is_dummy = true;
        }
    }

    uint64_t hash_to_bin(const zz_p& item, uint64_t func_idx) const
    {
        uint64_t item_val = static_cast<uint64_t>(conv<unsigned long>(item));
        uint64_t h = (item_val * (func_idx + 1) * 0x9e3779b97f4a7c15ULL) >> (64 - log2_ceil(b));
        return h % b;
    }

    bool insert(const zz_p& item, uint64_t max_evictions = 128)
    {
        CuckooHashEntry entry;
        entry.item = item;
        entry.is_dummy = false;

        for (uint64_t evict_count = 0; evict_count < max_evictions; evict_count++) {
            for (uint64_t j = 0; j < gamma; j++) {
                uint64_t bin = hash_to_bin(entry.item, j);
                if (table[bin].is_dummy) {
                    table[bin] = entry;
                    return true;
                }
            }

            uint64_t rand_func = (entry.item._zz_p__rep * 0x9e3779b97f4a7c15ULL) % gamma;
            uint64_t bin = hash_to_bin(entry.item, rand_func);
            std::swap(entry, table[bin]);
        }

        for (uint64_t i = 0; i < stash_sz; i++) {
            if (stash[i].is_dummy) {
                stash[i] = entry;
                return true;
            }
        }

        return false;
    }

    void pad_empty_bins(const zz_p& dummy_item)
    {
        CuckooHashEntry dummy;
        dummy.item = dummy_item;
        dummy.is_dummy = true;
        for (uint64_t i = 0; i < b; i++) {
            if (table[i].is_dummy) {
                table[i] = dummy;
            }
        }
    }

    uint64_t num_bins() const { return b; }
    uint64_t num_hash_funcs() const { return gamma; }

    const CuckooHashEntry& get_bin(uint64_t idx) const { return table[idx]; }

    std::vector<CuckooHashEntry> table;
    std::vector<CuckooHashEntry> stash;
    uint64_t b;
    uint64_t gamma;
    uint64_t stash_sz;

private:
    uint64_t log2_ceil(uint64_t n) const
    {
        uint64_t r = 0;
        while ((1ULL << r) < n) r++;
        return r;
    }
};

#endif
