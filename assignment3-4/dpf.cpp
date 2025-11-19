#include "dpf.h"
#include <cstring>

namespace cs670 {


uint64_t PRG::next_u64() {
    uint64_t x = state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    state = x;
    return x * 0x2545F4914F6CDD1DULL;
}

void PRG::fill_u64(std::vector<uint64_t>& out, size_t n) {
    out.clear();
    out.reserve(n);
    for (size_t i = 0; i < n; i++) {
        out.push_back(next_u64());
    }
}


std::vector<uint8_t> DPFKey::serialize() const {
    std::vector<uint8_t> buf;
    
    for (int i = 0; i < 8; i++)
        buf.push_back((prg_seed >> (i * 8)) & 0xFF);
    
    for (int i = 0; i < 4; i++)
        buf.push_back((tree_height >> (i * 8)) & 0xFF);
    
    for (int i = 0; i < 8; i++)
        buf.push_back((index >> (i * 8)) & 0xFF);
    
    uint64_t fcw_size = fcw.size();
    for (int i = 0; i < 8; i++)
        buf.push_back((fcw_size >> (i * 8)) & 0xFF);
    
    for (auto v : fcw) {
        uint64_t x = (uint64_t)v;
        for (int i = 0; i < 8; i++)
            buf.push_back((x >> (i * 8)) & 0xFF);
    }
    return buf;
}

DPFKey DPFKey::deserialize(const std::vector<uint8_t>& bytes) {
    DPFKey k;
    size_t pos = 0;

    auto read_u64 = [&](uint64_t &out) {
        out = 0;
        for (int i = 0; i < 8; i++)
            out |= (uint64_t(bytes[pos++]) << (i * 8));
    };
    auto read_u32 = [&](uint32_t &out) {
        out = 0;
        for (int i = 0; i < 4; i++)
            out |= (uint32_t(bytes[pos++]) << (i * 8));
    };

    read_u64(k.prg_seed);
    read_u32(k.tree_height);

    uint64_t idx_val;
    read_u64(idx_val);
    k.index = idx_val;

    uint64_t fcw_size;
    read_u64(fcw_size);
    k.fcw.resize(fcw_size);

    for (uint64_t idx = 0; idx < fcw_size; idx++) {
        uint64_t tmp;
        read_u64(tmp);
        k.fcw[idx] = (FieldT)tmp;
    }
    return k;
}

std::pair<DPFKey, DPFKey> Gen_point_zero(
    DomainIndex idx,
    uint32_t tree_height,
    uint32_t vector_dim,
    std::mt19937_64& rng)
{
    DPFKey k0, k1;

    
    k0.prg_seed = rng();
    k1.prg_seed = k0.prg_seed; 

    k0.tree_height = tree_height;
    k1.tree_height = tree_height;

    k0.index = idx;
    k1.index = idx;

    k0.fcw.assign(vector_dim, FieldT(0));
    k1.fcw.assign(vector_dim, FieldT(0));

    return {k0, k1};
}


std::vector<FieldT> EvalFull(const DPFKey& key) {
    uint64_t domain_size = domain_size_from_height(key.tree_height);
    uint32_t vector_dim = (uint32_t)key.fcw.size();

    uint64_t total = domain_size * vector_dim;

    PRG prg(key.prg_seed);
    std::vector<uint64_t> raw_u64;
    prg.fill_u64(raw_u64, total);

    std::vector<FieldT> out(total);
    for (uint64_t i = 0; i < total; i++) {
        out[i] = (FieldT)(raw_u64[i] & 0x7FFFFFFFFFFFFFFFULL); 
    }


    uint64_t target = key.index;
    if (target < domain_size) {
        for (uint32_t d = 0; d < vector_dim; d++) {
            uint64_t idx2 = target * vector_dim + d;
            out[idx2] += key.fcw[d];
        }
    }

    return out;
}

} 
