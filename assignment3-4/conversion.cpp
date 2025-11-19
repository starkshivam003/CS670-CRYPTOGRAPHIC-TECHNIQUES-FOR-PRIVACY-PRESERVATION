#include "conversion.h"
#include <cassert>

namespace cs670 {

// ------------------------------------------------------------
// Baseline (insecure) XOR→additive conversion
// ------------------------------------------------------------
void baseline_xor_to_additive(
    const std::vector<FieldT>& D_b,
    int party_id,
    std::vector<FieldT>& out)
{
    out.resize(D_b.size());

    // Party 0: A0 = D0
    // Party 1: A1 = -D1
    if (party_id == 0) {
        for (size_t i = 0; i < D_b.size(); i++)
            out[i] = D_b[i];
    } else {
        for (size_t i = 0; i < D_b.size(); i++)
            out[i] = -D_b[i];
    }
}

// ------------------------------------------------------------
// Secure XOR→additive conversion using simulated Beaver-style masks
// ------------------------------------------------------------

void secure_xor_to_additive_beaver(
    const std::vector<FieldT>& D_b,
    int party_id,
    const std::vector<FieldT>& R_b,
    std::vector<FieldT>& out_share,
    std::vector<FieldT>* mailbox_to_other,
    const std::vector<FieldT>* mailbox_from_other)
{
    const size_t n = D_b.size();
    assert(R_b.size() == n);

    std::vector<FieldT> M_b(n);
    for (size_t i = 0; i < n; i++) {
        M_b[i] = D_b[i] + R_b[i];
    }

    if (mailbox_to_other) {
        (*mailbox_to_other) = M_b;
    }

    assert(mailbox_from_other != nullptr);
    const std::vector<FieldT>& M_other = *mailbox_from_other;
    assert(M_other.size() == n);

    std::vector<FieldT> M(n);
    for (size_t i = 0; i < n; i++) {
        M[i] = M_b[i] + M_other[i];
    }

    out_share.resize(n);

    if (party_id == 0) {
        
        std::random_device rd;
        std::mt19937_64 rng(rd());
        for (size_t i = 0; i < n; i++) {
            
            FieldT S0 = (FieldT)(rng() & 0x7FFFFFFFFFFFFFFFLL);
            out_share[i] = S0;
        }
        
        if (mailbox_to_other) {
            (*mailbox_to_other) = out_share;  
        }

    } else {
        
        assert(mailbox_from_other != nullptr);
        const std::vector<FieldT>& S0 = *mailbox_from_other;
        assert(S0.size() == n);

        
        for (size_t i = 0; i < n; i++) {
            out_share[i] = M[i] - S0[i];
        }
    }

 

    for (size_t i = 0; i < n; i++) {
        out_share[i] -= R_b[i];   // remove mask locally
    }
}

} 
