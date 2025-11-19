#include "dpf.h"
#include "conversion.h"

#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <cstring>
#include <cassert>

using namespace cs670;


struct BenchArgs {
    uint64_t num_items = 1024;
    uint32_t vector_dim = 4;
    uint32_t tree_height = 10;  
    uint32_t runs = 30; 
};

static BenchArgs parse_args(int argc, char** argv) {
    BenchArgs a;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--items") && i + 1 < argc) a.num_items = std::stoull(argv[++i]);
        else if (!strcmp(argv[i], "--dim") && i + 1 < argc) a.vector_dim = std::stoul(argv[++i]);
        else if (!strcmp(argv[i], "--runs") && i + 1 < argc) a.runs = std::stoul(argv[++i]);
        else if (!strcmp(argv[i], "--height") && i + 1 < argc) a.tree_height = std::stoul(argv[++i]);
    }
    return a;
}


inline uint64_t flat(uint64_t idx, uint32_t dim, uint32_t d) {
    return idx * (uint64_t)dim + d;
}

// ---------------------------
// Random fixed-point vector
// ---------------------------
std::vector<FieldT> rand_vec_fp(uint32_t dim, std::mt19937_64& rng) {
    std::vector<FieldT> v(dim);
    for (uint32_t i = 0; i < dim; i++) {
        double r = std::uniform_real_distribution<double>(-1.0, 1.0)(rng);
        v[i] = (FieldT)std::llround(r * (double)SCALE);
    }
    return v;
}

// ---------------------------
// Benchmark single update
// ---------------------------
void bench_update(const BenchArgs& args) {
    uint64_t N = args.num_items;
    uint32_t dim = args.vector_dim;
    uint32_t height = args.tree_height;

    if ((1ULL << height) < N) {
        std::cerr << "Error: tree_height is insufficient for num_items, kindly enter a valid height.\n";
        return;
    }

    std::mt19937_64 rng(std::random_device{}());

    // Prepare statistics
    std::vector<uint64_t> secure_times, user_times;

    for (uint32_t run = 0; run < args.runs; run++) {
        // Random index for update
        uint64_t j = std::uniform_int_distribution<uint64_t>(0, N - 1)(rng);
        // Random user vector
        std::vector<FieldT> u = rand_vec_fp(dim, rng);

        // --- Secure item update timing (DPF EvalFull) ---
        
        auto keys = Gen_point_zero(j, height, dim, rng);
        
        std::vector<FieldT> fcw(dim);
        for (uint32_t d = 0; d < dim; d++) fcw[d] = u[d]; 
        keys.first.fcw = fcw;
        keys.second.fcw = fcw;

        // Time DPF EvalFull
        auto start_secure = std::chrono::high_resolution_clock::now();
        std::vector<FieldT> D0 = EvalFull(keys.first);
        std::vector<FieldT> D1 = EvalFull(keys.second);
        auto end_secure = std::chrono::high_resolution_clock::now();
        uint64_t secure_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_secure - start_secure).count();
        secure_times.push_back(secure_ns);

        // --- User-local update timing ---
        std::vector<FieldT> user_vec(dim);
        auto start_user = std::chrono::high_resolution_clock::now();
        for (uint32_t d = 0; d < dim; d++) user_vec[d] += u[d];
        auto end_user = std::chrono::high_resolution_clock::now();
        uint64_t user_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_user - start_user).count();
        user_times.push_back(user_ns);
    }

    // Compute statistics
    auto stats = [](const std::vector<uint64_t>& v) {
        double avg = 0, minv = 1e18, maxv = 0, stddev = 0;
        for (auto x : v) {
            avg += x;
            if (x < minv) minv = x;
            if (x > maxv) maxv = x;
        }
        avg /= v.size();
        for (auto x : v) stddev += (x - avg) * (x - avg);
        stddev = sqrt(stddev / v.size());
        return std::make_tuple(avg, minv, maxv, stddev);
    };
    double s_avg, s_min, s_max, s_std, u_avg, u_min, u_max, u_std;
    std::tie(s_avg, s_min, s_max, s_std) = stats(secure_times);
    std::tie(u_avg, u_min, u_max, u_std) = stats(user_times);

    // Output CSV row
    std::cout << N << "," << args.vector_dim << "," << args.runs << ","
              << s_avg << "," << s_min << "," << s_max << "," << s_std << ","
              << u_avg << "," << u_min << "," << u_max << "," << u_std << "\n";
}

int main(int argc, char** argv) {
    BenchArgs args = parse_args(argc, argv);

    // Output CSV header
    std::cout << "N,vector_dim,runs,secure_time_ns_avg,secure_time_ns_min,secure_time_ns_max,secure_time_ns_stddev,user_time_ns_avg,user_time_ns_min,user_time_ns_max,user_time_ns_stddev\n";
    std::vector<uint64_t> N_values = {50, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1200, 1500, 2000};
    std::vector<uint32_t> dims = {2, 4, 8};
    std::vector<uint32_t> runs_list = {20, 30, 50};
    for (auto dim : dims) {
        args.vector_dim = dim;
        for (auto runs : runs_list) {
            args.runs = runs;
            for (auto items : N_values) {
                args.num_items = items;
                // Set tree_height so that 2^tree_height >= num_items
                args.tree_height = 0;
                while ((1ULL << args.tree_height) < items) args.tree_height++;
                bench_update(args);
            }
        }
    }
    return 0;
}
