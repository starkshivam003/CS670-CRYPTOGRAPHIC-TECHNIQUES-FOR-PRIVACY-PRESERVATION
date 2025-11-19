#pragma once
#include "common.hpp"
#include <numeric>

inline int64 dot(const std::vector<int64> &a, const std::vector<int64> &b) {
    int64 s = 0;
    size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) s += a[i] * b[i];
    return s;
}

inline std::vector<int64> add_vec(const std::vector<int64> &a, const std::vector<int64> &b) {
    size_t n = std::max(a.size(), b.size());
    std::vector<int64> r(n);
    for (size_t i = 0; i < n; ++i) {
        int64 ai = (i < a.size()) ? a[i] : 0;
        int64 bi = (i < b.size()) ? b[i] : 0;
        r[i] = ai + bi;
    }
    return r;
}

inline std::vector<int64> sub_vec(const std::vector<int64> &a, const std::vector<int64> &b) {
    size_t n = std::max(a.size(), b.size());
    std::vector<int64> r(n);
    for (size_t i = 0; i < n; ++i) {
        int64 ai = (i < a.size()) ? a[i] : 0;
        int64 bi = (i < b.size()) ? b[i] : 0;
        r[i] = ai - bi;
    }
    return r;
}

inline std::vector<int64> mul_vec_scalar(const std::vector<int64> &v, int64 s) {
    std::vector<int64> r(v.size());
    for (size_t i = 0; i < v.size(); ++i) r[i] = v[i] * s;
    return r;
}
#pragma once
#include "shares.hpp"
#include "common.hpp"