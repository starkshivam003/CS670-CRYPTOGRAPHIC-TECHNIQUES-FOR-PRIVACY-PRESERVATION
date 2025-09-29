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

    std::ofstream u0_file("output/U0_rep.txt"), u1_file("output/U1_rep.txt"), u2_file("output/U2_rep.txt");
    std::ofstream v0_file("output/V0_rep.txt"), v1_file("output/V1_rep.txt"), v2_file("output/V2_rep.txt");

    for (size_t i = 0; i < m; ++i) {
        SharedVector u_real, u0_s, u1_s, u2_s;
        u_real.resize(k); u0_s.resize(k); u1_s.resize(k); u2_s.resize(k);
        for (size_t j = 0; j < k; ++j) {
            u_real.data[j] = random_uint32(); u0_s.data[j] = random_uint32();
            u1_s.data[j] = random_uint32(); u2_s.data[j] = u_real.data[j] - u0_s.data[j] - u1_s.data[j];
        }
        
        write_vector_to_file(u0_file, u0_s); write_vector_to_file(u0_file, u1_s);
        write_vector_to_file(u1_file, u1_s); write_vector_to_file(u1_file, u2_s);
        write_vector_to_file(u2_file, u2_s); write_vector_to_file(u2_file, u0_s);
    }

    for (size_t i = 0; i < n; ++i) {
        SharedVector v_real, v0_s, v1_s, v2_s;
        v_real.resize(k); v0_s.resize(k); v1_s.resize(k); v2_s.resize(k);
        for (size_t j = 0; j < k; ++j) {
            v_real.data[j] = random_uint32(); v0_s.data[j] = random_uint32();
            v1_s.data[j] = random_uint32(); v2_s.data[j] = v_real.data[j] - v0_s.data[j] - v1_s.data[j];
        }
        write_vector_to_file(v0_file, v0_s); write_vector_to_file(v0_file, v1_s);
        write_vector_to_file(v1_file, v1_s); write_vector_to_file(v1_file, v2_s);
        write_vector_to_file(v2_file, v2_s); write_vector_to_file(v2_file, v0_s);
    }
    std::cout << "Replicated data generation complete! Files are in the output/ directory." << std::endl;
    return 0;
}