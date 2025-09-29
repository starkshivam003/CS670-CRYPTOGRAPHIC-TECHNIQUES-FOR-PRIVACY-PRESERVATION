#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "common.hpp" 
#include "shares.hpp"

void write_vector_to_file(std::ofstream& file, const SharedVector& vec) {
    for (size_t i = 0; i < vec.size(); ++i) {
        file << vec.data[i] << (i == vec.size() - 1 ? "" : " ");
    }
    file << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <num_users> <num_items> <num_features>" << std::endl;
        return 1;
    }
    size_t m = std::stoul(argv[1]);
    size_t n = std::stoul(argv[2]);
    size_t k = std::stoul(argv[3]);

    std::ofstream u0_file("output/U0.txt"), u1_file("output/U1.txt");
    std::ofstream v0_file("output/V0.txt"), v1_file("output/V1.txt");

    for (size_t i = 0; i < m; ++i) {
        SharedVector u_real, u0_share, u1_share;
        u_real.resize(k); u0_share.resize(k); u1_share.resize(k);
        for (size_t j = 0; j < k; ++j) {
            u_real.data[j] = random_uint32();
            u0_share.data[j] = random_uint32();
            u1_share.data[j] = u_real.data[j] - u0_share.data[j];
        }
        write_vector_to_file(u0_file, u0_share);
        write_vector_to_file(u1_file, u1_share);
    }

    for (size_t i = 0; i < n; ++i) {
        SharedVector v_real, v0_share, v1_share;
        v_real.resize(k); v0_share.resize(k); v1_share.resize(k);
        for (size_t j = 0; j < k; ++j) {
            v_real.data[j] = random_uint32();
            v0_share.data[j] = random_uint32();
            v1_share.data[j] = v_real.data[j] - v0_share.data[j];
        }
        write_vector_to_file(v0_file, v0_share);
        write_vector_to_file(v1_file, v1_share);
    }
    std::cout << "Data generation complete! Files are in output/ directory." << std::endl;
    return 0;
}