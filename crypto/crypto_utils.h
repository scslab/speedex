#pragma once

/*! \file crypto_utils.h

Utility functions for signing blocks of transactions and managing public keys
in a deterministic manner.  Only useful for running simulations (deterministic
keys makes setup vastly simpler)
*/

#include "xdr/types.h"
#include "xdr/block.h"

#include <sodium.h>
#include <array>

#include "speedex/speedex_management_structures.h"

namespace speedex {

class BlockSignatureChecker {

	SpeedexManagementStructures& management_structures;

public:
	BlockSignatureChecker(SpeedexManagementStructures& management_structures) 
	: management_structures(management_structures) {
		if (sodium_init() == -1) {
			throw std::runtime_error("could not init sodium");
		}
	}

	bool check_all_sigs(const SerializedBlock& block);
};


struct DeterministicKeyGenerator {

	DeterministicKeyGenerator() {
		if (sodium_init() == -1) {
			throw std::runtime_error("could not init sodium");
		}
	}

	std::pair<SecretKey, PublicKey> 
	deterministic_key_gen(uint64_t seed);

	std::pair<std::vector<SecretKey>, std::vector<PublicKey>>
	gen_key_pair_list(AccountID num_accounts);
};

} /* speedex */