#pragma once
#include <random>
#include <cstdint>

#include <utility>

inline uint64_t random_uint64() {
    static std::mt19937_64 rng(std::random_device{}());
    static std::uniform_int_distribution<uint64_t> dist(0, (1ULL << 61) - 2);
    return dist(rng);
}

inline uint32_t random_uint32() {
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);
    return dist(rng);
}
