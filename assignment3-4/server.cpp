#include "dpf.h"
#include "conversion.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <cassert>
#include <chrono>
#include <cstring>

using namespace cs670;

// -----------------------------
// Helpers: file IO for DPFKey
// -----------------------------
static bool read_file_bytes(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs) return false;
    std::streamsize sz = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    out.resize((size_t)sz);
    if (!ifs.read(reinterpret_cast<char*>(out.data()), sz)) return false;
    return true;
}

inline uint64_t flat_index(uint64_t item_idx, uint32_t dim, uint32_t coord) {
    return item_idx * (uint64_t)dim + (uint64_t)coord;
}

FieldT dot_prod(const std::vector<FieldT>& a, const std::vector<FieldT>& b, size_t offset_a = 0, size_t offset_b = 0, size_t len = 0) {
    if (len == 0) len = a.size() - offset_a;
    FieldT acc = 0;
    for (size_t i = 0; i < len; ++i) {
        acc += a[offset_a + i] * b[offset_b + i];
    }
    return acc;
}

std::vector<FieldT> fp_from_double_vec(const std::vector<double>& u) {
    std::vector<FieldT> out(u.size());
    for (size_t i = 0; i < u.size(); ++i) {
        out[i] = (FieldT)std::llround(u[i] * (double)SCALE);
    }
    return out;
}

std::vector<double> fp_to_double_vec(const std::vector<FieldT>& v) {
    std::vector<double> out(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
        out[i] = (double)v[i] / (double)SCALE;
    }
    return out;
}

std::vector<FieldT> reconstruct_item(const std::vector<FieldT>& V0, const std::vector<FieldT>& V1, uint32_t vector_dim, uint64_t j) {
    std::vector<FieldT> out(vector_dim);
    for (uint32_t d = 0; d < vector_dim; ++d) {
        out[d] = V0[flat_index(j, vector_dim, d)] + V1[flat_index(j, vector_dim, d)];
    }
    return out;
}

int main(int argc, char** argv) {
    if (argc < 6) {
        std::cerr << "Usage: " << argv[0] << " <k0_file> <k1_file> <vector_dim> <tree_height> <index_j>\n";
        return 1;
    }

    std::string k0_file = argv[1];
    std::string k1_file = argv[2];
    uint32_t vector_dim = static_cast<uint32_t>(std::stoul(argv[3]));
    uint32_t tree_height = static_cast<uint32_t>(std::stoul(argv[4]));
    uint64_t idx_j = std::stoull(argv[5]);

    std::vector<uint8_t> kb0_bytes, kb1_bytes;
    if (!read_file_bytes(k0_file, kb0_bytes) || !read_file_bytes(k1_file, kb1_bytes)) {
        std::cerr << "Error reading key files.\n";
        return 1;
    }
    DPFKey k0 = DPFKey::deserialize(kb0_bytes);
    DPFKey k1 = DPFKey::deserialize(kb1_bytes);

    if (k0.fcw.size() != vector_dim || k1.fcw.size() != vector_dim) {
        std::cerr << "Key FCW vector_dim mismatch. Expected " << vector_dim << "\n";
        return 1;
    }

    uint64_t domain_size = domain_size_from_height(tree_height);
    if (idx_j >= domain_size) {
        std::cerr << "Index j out of domain range.\n";
        return 1;
    }

    // ---------------------------
    // Setup toy item database (additive shares V0, V1)
    // ---------------------------
    std::vector<FieldT> V0(domain_size * (uint64_t)vector_dim);
    std::vector<FieldT> V1(domain_size * (uint64_t)vector_dim);

    std::mt19937_64 rng(0xC0FFEE);
    for (uint64_t t = 0; t < domain_size; ++t) {
        std::vector<double> item_d(vector_dim);
        for (uint32_t d = 0; d < vector_dim; ++d) {
            item_d[d] = (double)(t + 1) + 0.1 * double(d + 1); 
        }
        std::vector<FieldT> item_fp = fp_from_double_vec(item_d);
        for (uint32_t d = 0; d < vector_dim; ++d) {
            FieldT r = (FieldT)(rng() & 0x7FFFFFFFFFFFFFFFLL);
            V0[flat_index(t, vector_dim, d)] = r;
            V1[flat_index(t, vector_dim, d)] = item_fp[d] - r;
        }
    }

    std::vector<double> u_real_double(vector_dim);
    for (uint32_t d = 0; d < vector_dim; ++d) {
        u_real_double[d] = 0.2 * (double)(d + 1); 
    }
    std::vector<FieldT> u_fp = fp_from_double_vec(u_real_double); 

    std::vector<FieldT> V0_item(vector_dim), V1_item(vector_dim);
    for (uint32_t d = 0; d < vector_dim; ++d) {
        V0_item[d] = V0[flat_index(idx_j, vector_dim, d)];
        V1_item[d] = V1[flat_index(idx_j, vector_dim, d)];
    }

    FieldT alpha0 = dot_prod(u_fp, V0_item, 0, 0, vector_dim);
    FieldT alpha1 = dot_prod(u_fp, V1_item, 0, 0, vector_dim);
  
    std::vector<FieldT> M0(vector_dim), M1(vector_dim);
    for (uint32_t d = 0; d < vector_dim; ++d) {
        FieldT term_u = u_fp[d];
        M0[d] = term_u - (u_fp[d] * alpha0) / (FieldT)SCALE; 
        M1[d] = - (u_fp[d] * alpha1) / (FieldT)SCALE;
    }

    std::vector<FieldT> masked0(vector_dim), masked1(vector_dim);
    for (uint32_t d = 0; d < vector_dim; ++d) {
        masked0[d] = M0[d] - k0.fcw[d];
        masked1[d] = M1[d] - k1.fcw[d];
    }

    std::vector<FieldT> FCW_m(vector_dim);
    for (uint32_t d = 0; d < vector_dim; ++d) {
        FCW_m[d] = masked0[d] + masked1[d]; 
    }

    k0.fcw = masked0;
    k1.fcw.resize(vector_dim);
    for (uint32_t d = 0; d < vector_dim; d++) k1.fcw[d] = (FieldT)0 - masked1[d];


    std::vector<FieldT> D0 = EvalFull(k0);
    std::vector<FieldT> D1 = EvalFull(k1);
    std::vector<FieldT> A0_baseline(D0.size()), A1_baseline(D1.size());
    baseline_xor_to_additive(D0, 0, A0_baseline);
    baseline_xor_to_additive(D1, 1, A1_baseline);

    for (size_t i = 0; i < V0.size(); ++i) {
        V0[i] = V0[i] + A0_baseline[i];
        V1[i] = V1[i] + A1_baseline[i];
    }

    std::vector<FieldT> vj_after = reconstruct_item(V0, V1, vector_dim, idx_j);

    std::vector<double> item_d_orig(vector_dim);
    for (uint32_t d = 0; d < vector_dim; ++d) {
        item_d_orig[d] = (double)(idx_j + 1) + 0.1 * double(d + 1);
    }
    std::vector<FieldT> item_fp_orig = fp_from_double_vec(item_d_orig);

    std::vector<FieldT> expected_M(vector_dim);
    FieldT alpha_full = alpha0 + alpha1;
    for (uint32_t d = 0; d < vector_dim; ++d) {
        expected_M[d] = (u_fp[d] * ((FieldT)SCALE - alpha_full)) / (FieldT)SCALE;
    }

    std::vector<FieldT> expected_v_after(vector_dim);
    for (uint32_t d = 0; d < vector_dim; ++d) {
        expected_v_after[d] = item_fp_orig[d] + expected_M[d];
    }

    std::vector<double> v_after_d = fp_to_double_vec(vj_after);
    std::vector<double> expected_after_d = fp_to_double_vec(expected_v_after);

    std::cout << "Reconstructed item j=" << idx_j << " after update (double):\n";
    for (uint32_t d = 0; d < vector_dim; ++d) {
        std::cout << " coord[" << d << "] = " << v_after_d[d] << "   expected = " << expected_after_d[d] << "\n";
    }

    double tol = 1e-3;
    bool ok = true;
    for (uint32_t d = 0; d < vector_dim; ++d) {
        if (std::fabs(v_after_d[d] - expected_after_d[d]) > tol) {
            ok = false;
        }
    }
    if (ok) {
        std::cout << "Verification PASSED (baseline conversion within tolerance).\n";
    } else {
        std::cout << "Verification FAILED (baseline conversion disagreement).\n";
    }

    return 0;
}
