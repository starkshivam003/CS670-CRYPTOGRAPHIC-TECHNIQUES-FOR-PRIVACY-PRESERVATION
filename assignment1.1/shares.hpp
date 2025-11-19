#pragma once
#include "common.hpp"
#include <fstream>
#include <string>

inline void write_shares(const std::string &path, const std::vector<int64> &user_share, const std::vector<int64> &item_share) {
    std::ofstream ofs(path);
    ofs << join_vec(user_share) << "\n";
    ofs << join_vec(item_share) << "\n";
}

inline bool read_shares(const std::string &path, std::vector<int64> &user_share, std::vector<int64> &item_share) {
    std::ifstream ifs(path);
    if (!ifs) return false;
    std::string line;
    if (!std::getline(ifs, line)) return false;
    user_share = parse_vec(line);
    if (!std::getline(ifs, line)) return false;
    item_share = parse_vec(line);
    return true;
}
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