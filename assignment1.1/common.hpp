#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>

using int64 = int64_t;
using u64 = uint64_t;

inline std::string join_vec(const std::vector<int64>& v) {
    std::ostringstream oss;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) oss << ' ';
        oss << v[i];
    }
    return oss.str();
}

inline std::vector<int64> parse_vec(const std::string &s) {
    std::istringstream iss(s);
    std::vector<int64> v; int64 x;
    while (iss >> x) v.push_back(x);
    return v;
}