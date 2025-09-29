#pragma once
#include <vector>
#include <cstdint>
#include "common.hpp"

struct SharedVector {
    std::vector<uint32_t> data;

    void resize(size_t size) {
        data.resize(size);
    }

    size_t size() const {
        return data.size();
    }
};