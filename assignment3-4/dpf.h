#ifndef CS670_DPF_H
#define CS670_DPF_H

#include <cstdint>
#include <vector>
#include <array>
#include <random>
#include <string>
#include <cassert>

namespace cs670 {

using FieldT = int64_t;
static constexpr FieldT SCALE = 1000000LL; 

using DomainIndex = uint64_t;

struct DPFKey {
    uint64_t prg_seed;           
    uint32_t tree_height;       
    DomainIndex index;            
    std::vector<FieldT> fcw;    
    std::vector<uint8_t> serialize() const;
    static DPFKey deserialize(const std::vector<uint8_t>& bytes);
};


class PRG {
public:
    explicit PRG(uint64_t seed) : state(seed ? seed : 0xdeadbeefcafebabeULL) {}
    uint64_t next_u64();
    void fill_u64(std::vector<uint64_t>& out, size_t n);
private:
    uint64_t state;
};

std::pair<DPFKey, DPFKey> Gen_point_zero(DomainIndex idx, uint32_t tree_height, uint32_t vector_dim, std::mt19937_64& rng);


std::vector<FieldT> EvalFull(const DPFKey& key);

inline uint64_t domain_size_from_height(uint32_t h) {
    return (1ULL << h);
}

inline std::vector<FieldT> alloc_zero_flat(uint32_t tree_height, uint32_t vector_dim) {
    uint64_t n = domain_size_from_height(tree_height);
    std::vector<FieldT> z;
    z.resize(n * vector_dim, FieldT(0));
    return z;
}

} 

#endif 
