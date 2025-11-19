#include "common.hpp"
#include "shares.hpp"
#include <random>
#include <filesystem>
#include <iostream>
#include <vector>

int main(int argc, char* argv[]) {
    // usage: ./gen_data <num_users> <num_items> <dim>
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <num_users> <num_items> <dim>\n";
        return 1;
    }
    int num_users = std::stoi(argv[1]); (void)num_users; // currently only supporting single user
    int num_items = std::stoi(argv[2]); (void)num_items;
    int dim = std::stoi(argv[3]);

    std::filesystem::create_directory("/workspace/output");

    std::mt19937_64 rng((unsigned)123456);
    std::uniform_int_distribution<int64> dist(-5,5);

    std::vector<int64> user(dim), item(dim);
    for (int i = 0; i < dim; ++i) { user[i] = dist(rng); item[i] = dist(rng); }

    // additive shares
    std::vector<int64> user0(dim), user1(dim), item0(dim), item1(dim);
    for (int i = 0; i < dim; ++i) {
        user0[i] = dist(rng);
        user1[i] = user[i] - user0[i];
        item0[i] = dist(rng);
        item1[i] = item[i] - item0[i];
    }

    write_shares("/workspace/output/share_p0_0.txt", user0, item0);
    write_shares("/workspace/output/share_p1_0.txt", user1, item1);

    std::cout << "Wrote initial shares to /workspace/output/share_p0_0.txt and share_p1_0.txt\n";
    return 0;
}