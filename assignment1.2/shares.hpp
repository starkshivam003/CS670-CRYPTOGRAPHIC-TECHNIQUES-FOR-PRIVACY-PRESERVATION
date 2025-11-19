
#pragma once
#include <vector>
#include <cstdint>

#include <utility>

using Field = uint64_t; 
constexpr Field MODULUS = (1ULL << 61) - 1; 

inline Field add_mod(Field a, Field b) {
    Field res = a + b;
    if (res >= MODULUS) res -= MODULUS;
    return res;
}

inline Field sub_mod(Field a, Field b) {
    Field res = (a >= b) ? (a - b) : (MODULUS + a - b);
    return res;
}

inline Field mul_mod(Field a, Field b) {
    return (a * b) % MODULUS;
}

struct ReplicatedShare {
    Field s_local;
    Field s_next;
    ReplicatedShare() : s_local(0), s_next(0) {}
    ReplicatedShare(Field a, Field b) : s_local(a), s_next(b) {}
};

using ReplicatedVector = std::vector<ReplicatedShare>;