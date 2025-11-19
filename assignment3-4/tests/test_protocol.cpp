// tests/test_protocol.cpp
// Unit test: verifies that one end-to-end update matches expected M addition.

#include "../dpf.h"
#include "../conversion.h"
#include <iostream>
#include <random>
#include <cassert>

using namespace cs670;

inline uint64_t flat(uint64_t idx, uint32_t dim, uint32_t d) {
    return idx * (uint64_t)dim + d;
}

int main() {
    std::mt19937_64 rng(123);

    uint32_t dim = 3;
    uint32_t height = 5; // 32 items
    uint64_t N = 1ULL << height;

    uint64_t j = 7; // test index

    // -----------------------------
    // User generates DPF keys for payload=0
    // -----------------------------
    auto keys = Gen_point_zero(j, height, dim, rng);
    DPFKey k0 = keys.first;
    DPFKey k1 = keys.second;

    // -----------------------------
    // Create additive shared item DB
    // -----------------------------
    std::vector<FieldT> V0(N * dim), V1(N * dim);
    for (uint64_t i = 0; i < N; i++) {
        for (uint32_t d = 0; d < dim; d++) {
            FieldT item = (FieldT)((i+1) * 10 + d);
            FieldT r = (FieldT)(rng() & 0x7FFFFFFFFFFFFFFFLL);
            V0[flat(i,dim,d)] = r;
            V1[flat(i,dim,d)] = item - r;
        }
    }

    // -----------------------------
    // User's u-vector
    // -----------------------------
    std::vector<FieldT> u(dim);
    for(uint32_t d=0; d<dim; d++) {
        u[d] = (FieldT)((d+1)*SCALE/10); // [0.1,0.2,0.3]
    }

    // -----------------------------
    // Compute shared M
    // -----------------------------
    std::vector<FieldT> V0j(dim), V1j(dim);
    for (uint32_t d=0; d<dim; d++) {
        V0j[d] = V0[flat(j,dim,d)];
        V1j[d] = V1[flat(j,dim,d)];
    }

    FieldT alpha0 = 0, alpha1 = 0;
    for (uint32_t d=0; d<dim; d++) {
        alpha0 += u[d] * V0j[d];
        alpha1 += u[d] * V1j[d];
    }
    FieldT alpha_full = alpha0 + alpha1;

    std::vector<FieldT> M0(dim), M1(dim);
    for (uint32_t d=0; d<dim; d++) {
        M0[d] = u[d] - (u[d]*alpha0)/SCALE;
        M1[d] = -(u[d]*alpha1)/SCALE;
    }

    // -----------------------------
    // Step 3: masked differences → FCW_m
    // -----------------------------
    std::vector<FieldT> masked0(dim), masked1(dim);

    for (uint32_t d=0; d<dim; d++) {
        masked0[d] = M0[d] - k0.fcw[d];
        masked1[d] = M1[d] - k1.fcw[d];
    }

    std::vector<FieldT> FCW_m(dim);
    for (uint32_t d=0; d<dim; d++) {
        FCW_m[d] = masked0[d] + masked1[d];
    }

    // Assign per-key FCW shares so that EvalFull(k0) - EvalFull(k1) = M
    // D0 = prg + k0.fcw, D1 = prg + k1.fcw => D0 - D1 = k0.fcw - k1.fcw
    // We want k0.fcw - k1.fcw = M, so set k0.fcw = masked0, k1.fcw = -masked1
    k0.fcw = masked0;
    k1.fcw.resize(dim);
    for (uint32_t d = 0; d < dim; d++) k1.fcw[d] = (FieldT)0 - masked1[d];

    // -----------------------------
    // Step 4: EvalFull + baseline conversion
    // -----------------------------
    auto D0 = EvalFull(k0);
    auto D1 = EvalFull(k1);

    std::vector<FieldT> A0, A1;
    baseline_xor_to_additive(D0, 0, A0);
    baseline_xor_to_additive(D1, 1, A1);

    // Add updates
    for (size_t i=0; i<V0.size(); i++) {
        V0[i] += A0[i];
        V1[i] += A1[i];
    }

    // -----------------------------
    // Reconstruct and verify
    // -----------------------------
    std::vector<FieldT> v_after(dim);
    std::vector<FieldT> v_orig(dim);
    std::vector<FieldT> M_expected(dim);

    // Original item
    for(uint32_t d=0; d<dim; d++){
        v_orig[d] = V0j[d] + V1j[d];
    }

    // Expected M
    for(uint32_t d=0; d<dim; d++){
        M_expected[d] = (u[d] * (SCALE - alpha_full)) / SCALE;
    }

    // Expected new item = orig + M
    std::vector<FieldT> expected(dim);
    for(uint32_t d=0; d<dim; d++){
        expected[d] = v_orig[d] + M_expected[d];
    }

    // Actual new item
    for(uint32_t d=0; d<dim; d++){
        v_after[d] = V0[flat(j,dim,d)] + V1[flat(j,dim,d)];
    }

    bool ok = true;
    for(uint32_t d=0; d<dim; d++){
        if (llabs(v_after[d] - expected[d]) > (FieldT)(SCALE/1000)) ok = false;
    }

    if(ok){
        std::cout << "TEST PASSED\n";
        return 0;
    } else {
        std::cout << "TEST FAILED\n";
        return 1;
    }
}
