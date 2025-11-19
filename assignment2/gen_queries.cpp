#include <openssl/evp.h>
#include <openssl/rand.h>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>
#include <string>
#include <array>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <unordered_set>


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

struct PRGSingle {
    uint128_t s;
    bool t;
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

// Helper: generate a cryptographically secure 128-bit random value
static uint128_t secure_rand128() {
    unsigned char buf[16];
    if (1 != RAND_bytes(buf, sizeof(buf))) {
        throw runtime_error("RAND_bytes failed");
    }
    return bytes_be_to_u128(buf);
}

// Helper: generate a cryptographically secure 64-bit random value
static uint64_t secure_rand64() {
    unsigned char buf[8];
    if (1 != RAND_bytes(buf, sizeof(buf))) {
        throw runtime_error("RAND_bytes failed");
    }
    uint64_t x = 0;
    for (int i = 0; i < 8; ++i) {
        x = (x << 8) | buf[i];
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



// PRG expansion following slides: PRG(seed || bit) -> child seed and bit
static PRGSingle prg_expand_bit(uint128_t seed, unsigned char bit) {
    array<unsigned char,17> in;
    auto sbytes = u128_to_bytes_be(seed);
    copy(sbytes.begin(), sbytes.end(), in.begin());
    in[16] = bit; // domain separation by bit (0=left,1=right)

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

    PRGSingle out;
    out.s = bytes_be_to_u128(digest);
    out.t = static_cast<bool>(out.s & 1u);
    return out;
}

pair<DPFKey, DPFKey> generateDPF(size_t dpf_size, uint64_t location, uint128_t value) {
    if ((dpf_size & (dpf_size - 1)) != 0) throw invalid_argument("dpf_size must be power of two");
    int depth = 0;
    {
        size_t tmp = dpf_size;
        while (tmp > 1) { tmp >>= 1; ++depth; }
    }

    DPFKey k0, k1;
    k0.levels.resize(depth);
    k1.levels.resize(depth);
    // Use cryptographically secure random 128-bit root seeds
    k0.root_seed = secure_rand128();
    k1.root_seed = secure_rand128();

    k0.root_flag = 0;
    k1.root_flag = 1;

    uint128_t s0 = k0.root_seed;
    uint128_t s1 = k1.root_seed;
    bool f0 = k0.root_flag;
    bool f1 = k1.root_flag;

    for (int i = 0; i < depth; ++i) {
        // Expand seed into left/right using PRG(seed || bit)
        PRGSingle out0L = prg_expand_bit(s0, 0);
        PRGSingle out0R = prg_expand_bit(s0, 1);
        PRGSingle out1L = prg_expand_bit(s1, 0);
        PRGSingle out1R = prg_expand_bit(s1, 1);

        bool path_bit = ((location >> (depth - 1 - i)) & 1u);

        uint128_t sCW;
        bool tLcw, tRcw;
        if (!path_bit) { // target goes left
            sCW = out0R.s ^ out1R.s;
            tLcw = static_cast<bool>(out0L.t ^ out1L.t ^ path_bit ^ 1);
            tRcw = static_cast<bool>(out0R.t ^ out1R.t ^ path_bit);
            k0.levels[i].keep_is_left = true;
            k1.levels[i].keep_is_left = true;
        } else { // target goes right
            sCW = out0L.s ^ out1L.s;
            tLcw = static_cast<bool>(out0L.t ^ out1L.t ^ path_bit ^ 1);
            tRcw = static_cast<bool>(out0R.t ^ out1R.t ^ path_bit);
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
                s0 = out0L.s;
                f0 = out0L.t;
            } else {
                s0 = out0R.s;
                f0 = out0R.t;
            }
        } else {
            if (keep_left) {
                s0 = out0L.s ^ sCW;
                f0 = out0L.t ^ tLcw;
            } else {
                s0 = out0R.s ^ sCW;
                f0 = out0R.t ^ tRcw;
            }
        }
        if (!f1) {
            if (keep_left) {
                s1 = out1L.s;
                f1 = out1L.t;
            } else {
                s1 = out1R.s;
                f1 = out1R.t;
            }
        } else {
            if (keep_left) {
                s1 = out1L.s ^ sCW;
                f1 = out1L.t ^ tLcw;
            } else {
                s1 = out1R.s ^ sCW;
                f1 = out1R.t ^ tRcw;
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
        // Expand both children using PRG(seed || bit)
        PRGSingle outL = prg_expand_bit(s, 0);
        PRGSingle outR = prg_expand_bit(s, 1);

        if (f) {
            const DPFLevel& L = key.levels[i];
            outL.s ^= L.scw;
            outL.t = static_cast<bool>(outL.t ^ L.tL_cw);
            outR.s ^= L.scw;
            outR.t = static_cast<bool>(outR.t ^ L.tR_cw);
        }

        bool bit = ((index >> (depth - 1 - i)) & 1u);
        if (!bit) {
            s = outL.s;
            f = outL.t;
        } else {
            s = outR.s;
            f = outR.t;
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

bool EvalSample(const DPFKey& key0, const DPFKey& key1, size_t dpf_size, uint64_t location, uint128_t value, size_t sample_count) {
    if (sample_count == 0 || sample_count >= dpf_size - 1) return EvalFull(key0, key1, dpf_size, location, value);
    bool ok = true;
    // Check target
    uint128_t v0 = evalDPF(key0, location);
    uint128_t v1 = evalDPF(key1, location);
    uint128_t combined = v0 ^ v1;
    if (combined != value) {
        cerr << "  [FAIL] target " << location << ": expected " << uint128_to_string(value)
                  << ", got " << uint128_to_string(combined) << "\n";
        ok = false;
    }

    // Sample random non-target indices
    size_t found = 0;
    size_t attempts = 0;
    const size_t max_attempts = sample_count * 10 + 100;
    unordered_set<uint64_t> seen;
    while (found < sample_count && attempts < max_attempts) {
        ++attempts;
        uint64_t r = secure_rand64() % dpf_size;
        if (r == location) continue;
        if (seen.count(r)) continue;
        seen.insert(r);
        ++found;
        uint128_t a0 = evalDPF(key0, r);
        uint128_t a1 = evalDPF(key1, r);
        uint128_t comb = a0 ^ a1;
        if (comb != 0) {
            cerr << "  [FAIL] sampled non-target " << r << " got " << uint128_to_string(comb) << "\n";
            ok = false;
        }
    }
    if (found < sample_count) {
        cerr << "  [WARN] only sampled " << found << " indices (requested " << sample_count << ")\n";
    }
    return ok;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <DPF_size> <num_DPFs> [--verify-sample N]\n";
        return 1;
    }
    size_t dpf_size = stoull(argv[1]);
    if ((dpf_size & (dpf_size - 1)) != 0 || dpf_size == 0) {
        cerr << "Error: <DPF_size> must be a power of 2.\n";
        return 1;
    }
    int num_dpfs = stoi(argv[2]);

    // Optional argument parsing for --verify-sample
    size_t sample_size_arg = SIZE_MAX; 
    for (int ai = 3; ai < argc; ++ai) {
        if (string(argv[ai]) == "--verify-sample" && ai + 1 < argc) {
            sample_size_arg = stoull(argv[++ai]);
        } else {
            cerr << "Unknown option: " << argv[ai] << "\n";
            return 1;
        }
    }

    cout << "Generating and testing " << num_dpfs << " DPFs of size " << dpf_size << "...\n";

    size_t default_sample_size = (dpf_size > 65536) ? 1024 : 0;

    size_t sample_size = (sample_size_arg == SIZE_MAX) ? default_sample_size : sample_size_arg;

    for (int i = 0; i < num_dpfs; ++i) {
        uint64_t random_location = secure_rand64() % dpf_size;
        uint128_t random_value = (uint128_t)1;

        cout << "\n--- Test " << i + 1 << "/" << num_dpfs << " ---\n";
        cout << "Target Location: " << random_location << ", Target Value: " << uint128_to_string(random_value) << "\n";

        auto [k0, k1] = generateDPF(dpf_size, random_location, random_value);
        bool success;
        if (sample_size == 0) {
            success = EvalFull(k0, k1, dpf_size, random_location, random_value);
        } else {
            success = EvalSample(k0, k1, dpf_size, random_location, random_value, sample_size);
        }

        if (success) cout << "Test Passed\n";
        else cout << "Test Failed\n";
    }
    return 0;
}
