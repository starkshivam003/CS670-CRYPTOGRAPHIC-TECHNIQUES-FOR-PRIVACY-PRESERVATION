
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "common.hpp"
#include "shares.hpp"

#include <utility>

void write_repvec_to_file(std::ofstream& file, const ReplicatedVector& vec) {
    for (const auto& share : vec) {
        file << share.s_local << " ";
    }
    file << std::endl;
    for (const auto& share : vec) {
        file << share.s_next << " ";
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

    std::ofstream u0_file("output/U0_rep.txt"), u1_file("output/U1_rep.txt"), u2_file("output/U2_rep.txt");
    std::ofstream v0_file("output/V0_rep.txt"), v1_file("output/V1_rep.txt"), v2_file("output/V2_rep.txt");

    for (size_t i = 0; i < m; ++i) {
        ReplicatedVector u0(k), u1(k), u2(k);
        for (size_t j = 0; j < k; ++j) {
            Field s0 = random_uint64() % MODULUS;
            Field s1 = random_uint64() % MODULUS;
            Field s2 = random_uint64() % MODULUS;
 
            u0[j] = ReplicatedShare(s0, s1);
            u1[j] = ReplicatedShare(s1, s2);
            u2[j] = ReplicatedShare(s2, s0);
        }
        write_repvec_to_file(u0_file, u0);
        write_repvec_to_file(u1_file, u1);
        write_repvec_to_file(u2_file, u2);
    }

    for (size_t i = 0; i < n; ++i) {
        ReplicatedVector v0(k), v1(k), v2(k);
        for (size_t j = 0; j < k; ++j) {
            Field s0 = random_uint64() % MODULUS;
            Field s1 = random_uint64() % MODULUS;
            Field s2 = random_uint64() % MODULUS;
            v0[j] = ReplicatedShare(s0, s1);
            v1[j] = ReplicatedShare(s1, s2);
            v2[j] = ReplicatedShare(s2, s0);
        }
        write_repvec_to_file(v0_file, v0);
        write_repvec_to_file(v1_file, v1);
        write_repvec_to_file(v2_file, v2);
    }
    std::cout << "Replicated data generation complete! Files are in the output/ directory." << std::endl;
    return 0;
}