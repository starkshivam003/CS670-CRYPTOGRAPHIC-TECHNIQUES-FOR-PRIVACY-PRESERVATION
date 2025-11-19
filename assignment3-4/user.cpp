#include "dpf.h"
#include <iostream>
#include <fstream>
#include <random>
#include <string>

using namespace cs670;

static void write_bytes_to_file(const std::string& path, const std::vector<uint8_t>& bytes) {
    std::ofstream ofs(path, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    ofs.close();
}

int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " <index> <tree_height> <vector_dim> <out_prefix>\n";
        return 1;
    }

    uint64_t idx = std::stoull(argv[1]);
    uint32_t tree_height = static_cast<uint32_t>(std::stoul(argv[2]));
    uint32_t vector_dim = static_cast<uint32_t>(std::stoul(argv[3]));
    std::string out_prefix = argv[4];

    std::random_device rd;
    std::mt19937_64 rng(rd());

    auto keys = Gen_point_zero(idx, tree_height, vector_dim, rng);
    DPFKey k0 = keys.first;
    DPFKey k1 = keys.second;

    auto b0 = k0.serialize();
    auto b1 = k1.serialize();

    std::string f0 = out_prefix + "_k0.bin";
    std::string f1 = out_prefix + "_k1.bin";

    write_bytes_to_file(f0, b0);
    write_bytes_to_file(f1, b1);

    std::cerr << "Wrote keys to: " << f0 << " , " << f1 << "\n";
    return 0;
}
