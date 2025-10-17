#include <openssl/evp.h>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>
#include <string>
#include <array>
#include <algorithm>
#include <cassert>
#include <cmath>


using uint128_t = __uint128_t;
using namespace std;


struct DPFLevel {
    uint128_t scw;   
    bool tL_cw;      
    bool tR_cw;      
    bool keep_is_left; 
};

struct DPFKey {
    uint128_t root_seed;
    bool root_flag;
    vector<DPFLevel> levels;
    uint128_t final_cw;
};

struct PRGOutput {
    uint128_t sL;
    bool tL;
    uint128_t sR;
    bool tR;
};



inline array<unsigned char,16> u128_to_bytes_be(uint128_t x) {
    array<unsigned char,16> b;
    for (int i = 15; i >= 0; --i) {
        b[i] = static_cast<unsigned char>(x & 0xFFu);
        x >>= 8;
    }
    return b;
}

inline uint128_t bytes_be_to_u128(const unsigned char *buf) {
    uint128_t x = 0;
    for (int i = 0; i < 16; ++i) {
        x <<= 8;
        x |= static_cast<uint128_t>(buf[i]);
    }
    return x;
}

string uint128_to_string(uint128_t n) {
    if (n == 0) return "0";
    string s;
    while (n > 0) {
        int digit = static_cast<int>(n % 10);
        s.push_back('0' + digit);
        n /= 10;
    }
    reverse(s.begin(), s.end());
    return s;
}



static PRGOutput prg_expand(uint128_t seed, unsigned int level) {
    array<unsigned char,17> in;
    auto sbytes = u128_to_bytes_be(seed);
    copy(sbytes.begin(), sbytes.end(), in.begin());
    in[16] = static_cast<unsigned char>(level & 0xFF);

    unsigned char digest[32];
    unsigned int digest_len = 0;

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (mdctx == nullptr) throw runtime_error("EVP_MD_CTX_new failed");
    if (1 != EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL)) {
        EVP_MD_CTX_free(mdctx);
        throw runtime_error("EVP_DigestInit_ex failed");
    }
    if (1 != EVP_DigestUpdate(mdctx, in.data(), in.size())) {
        EVP_MD_CTX_free(mdctx);
        throw runtime_error("EVP_DigestUpdate failed");
    }
    if (1 != EVP_DigestFinal_ex(mdctx, digest, &digest_len)) {
        EVP_MD_CTX_free(mdctx);
        throw runtime_error("EVP_DigestFinal_ex failed");
    }
    EVP_MD_CTX_free(mdctx);

    PRGOutput out;
    out.sL = bytes_be_to_u128(digest);
    out.sR = bytes_be_to_u128(digest + 16);
    out.tL = static_cast<bool>(out.sL & 1u); 
    out.tR = static_cast<bool>(out.sR & 1u);
    return out;
}

pair<DPFKey, DPFKey> generateDPF(size_t dpf_size, uint64_t location, uint128_t value) {
    if ((dpf_size & (dpf_size - 1)) != 0) throw invalid_argument("dpf_size must be power of two");
    int depth = static_cast<int>(log2((double)dpf_size));

    random_device rd;
    mt19937_64 rng(rd());
    auto rnd64 = [&]() { return (uint64_t)rng(); };

    DPFKey k0, k1;
    k0.levels.resize(depth);
    k1.levels.resize(depth);

    uint128_t r0_hi = (uint128_t)rnd64();
    uint128_t r0_lo = (uint128_t)rnd64();
    uint128_t r1_hi = (uint128_t)rnd64();
    uint128_t r1_lo = (uint128_t)rnd64();

    k0.root_seed = (r0_hi << 64) | r0_lo;
    k1.root_seed = (r1_hi << 64) | r1_lo;

    k0.root_flag = 0;
    k1.root_flag = 1;

    uint128_t s0 = k0.root_seed;
    uint128_t s1 = k1.root_seed;
    bool f0 = k0.root_flag;
    bool f1 = k1.root_flag;

    for (int i = 0; i < depth; ++i) {
        PRGOutput out0 = prg_expand(s0, (unsigned int)i);
        PRGOutput out1 = prg_expand(s1, (unsigned int)i);

        bool path_bit = ((location >> (depth - 1 - i)) & 1u);

      
        uint128_t sCW;
        bool tLcw, tRcw;
        if (!path_bit) { 
            sCW = out0.sR ^ out1.sR;
            tLcw = static_cast<bool>(out0.tL ^ out1.tL ^ path_bit ^ 1);
            tRcw = static_cast<bool>(out0.tR ^ out1.tR ^ path_bit);
            k0.levels[i].keep_is_left = true;
            k1.levels[i].keep_is_left = true;
        } else { 
            sCW = out0.sL ^ out1.sL;
            tLcw = static_cast<bool>(out0.tL ^ out1.tL ^ path_bit ^ 1);
            tRcw = static_cast<bool>(out0.tR ^ out1.tR ^ path_bit);
            k0.levels[i].keep_is_left = false;
            k1.levels[i].keep_is_left = false;
        }

        k0.levels[i].scw = sCW;
        k0.levels[i].tL_cw = tLcw;
        k0.levels[i].tR_cw = tRcw;
        
        k1.levels[i].scw = sCW;
        k1.levels[i].tL_cw = tLcw;
        k1.levels[i].tR_cw = tRcw;

        bool keep_left = !path_bit;
        if (!f0) {
            if (keep_left) {
                s0 = out0.sL;
                f0 = out0.tL;
            } else {
                s0 = out0.sR;
                f0 = out0.tR;
            }
        } else {
            if (keep_left) {
                s0 = out0.sL ^ sCW;
                f0 = out0.tL ^ tLcw;
            } else {
                s0 = out0.sR ^ sCW;
                f0 = out0.tR ^ tRcw;
            }
        }
        if (!f1) {
            if (keep_left) {
                s1 = out1.sL;
                f1 = out1.tL;
            } else {
                s1 = out1.sR;
                f1 = out1.tR;
            }
        } else {
            if (keep_left) {
                s1 = out1.sL ^ sCW;
                f1 = out1.tL ^ tLcw;
            } else {
                s1 = out1.sR ^ sCW;
                f1 = out1.tR ^ tRcw;
            }
        }
    }

    k0.final_cw = value ^ s0 ^ s1;
    k1.final_cw = k0.final_cw;

    return {k0, k1};
}

uint128_t evalDPF(const DPFKey& key, uint64_t index) {
    int depth = static_cast<int>(key.levels.size());
    uint128_t s = key.root_seed;
    bool f = key.root_flag;

    for (int i = 0; i < depth; ++i) {
        PRGOutput out = prg_expand(s, (unsigned int)i);

        if (f) {
            const DPFLevel& L = key.levels[i];
            out.sL ^= L.scw;
            out.tL = static_cast<bool>(out.tL ^ L.tL_cw);
            out.sR ^= L.scw;
            out.tR = static_cast<bool>(out.tR ^ L.tR_cw);
        }

        bool bit = ((index >> (depth - 1 - i)) & 1u);
        if (!bit) {
            s = out.sL;
            f = out.tL;
        } else {
            s = out.sR;
            f = out.tR;
        }
    }

    return s ^ (f ? key.final_cw : (uint128_t)0);
}

bool EvalFull(const DPFKey& key0, const DPFKey& key1, size_t dpf_size, uint64_t location, uint128_t value) {
    bool ok = true;
    for (uint64_t i = 0; i < dpf_size; ++i) {
        uint128_t v0 = evalDPF(key0, i);
        uint128_t v1 = evalDPF(key1, i);
        uint128_t combined = v0 ^ v1;
        if (i == location) {
            if (combined != value) {
                cerr << "  [FAIL] target " << i << ": expected " << uint128_to_string(value)
                          << ", got " << uint128_to_string(combined) << "\n";
                ok = false;
            }
        } else {
            if (combined != 0) {
                cerr << "  [FAIL] non-target " << i << " got " << uint128_to_string(combined) << "\n";
                ok = false;
            }
        }
    }
    return ok;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <DPF_size> <num_DPFs>\n";
        return 1;
    }
    size_t dpf_size = stoull(argv[1]);
    if ((dpf_size & (dpf_size - 1)) != 0 || dpf_size == 0) {
        cerr << "Error: <DPF_size> must be a power of 2.\n";
        return 1;
    }
    int num_dpfs = stoi(argv[2]);

    cout << "Generating and testing " << num_dpfs << " DPFs of size " << dpf_size << "...\n";

    random_device rd;
    mt19937_64 gen(rd());
    uniform_int_distribution<uint64_t> loc_dist(0, dpf_size - 1);

    for (int i = 0; i < num_dpfs; ++i) {
        uint64_t random_location = loc_dist(gen);
        uint128_t random_value = (uint128_t)1; 

        cout << "\n--- Test " << i + 1 << "/" << num_dpfs << " ---\n";
        cout << "Target Location: " << random_location << ", Target Value: " << uint128_to_string(random_value) << "\n";

        auto [k0, k1] = generateDPF(dpf_size, random_location, random_value);
        bool success = EvalFull(k0, k1, dpf_size, random_location, random_value);

        if (success) cout << "Test Passed\n";
        else cout << "Test Failed\n";
    }
    return 0;
}
