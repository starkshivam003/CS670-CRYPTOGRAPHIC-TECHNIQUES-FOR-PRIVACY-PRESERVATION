#ifndef CS670_CONVERSION_H
#define CS670_CONVERSION_H

#include <vector>
#include <cstdint>
#include "dpf.h"

namespace cs670 {


void baseline_xor_to_additive(
    const std::vector<FieldT>& D_b,
    int party_id,            
    std::vector<FieldT>& out
);


void secure_xor_to_additive_beaver(
    const std::vector<FieldT>& D_b,
    int party_id,                
    const std::vector<FieldT>& R_b,   
    std::vector<FieldT>& out_share,   
    std::vector<FieldT>* mailbox_to_other, 
    const std::vector<FieldT>* mailbox_from_other 
);

} 

#endif 
