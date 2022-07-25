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

#include <xdrpp/marshal.h>

namespace speedex {

void 
__attribute__((constructor)) 
initialize_crypto();

class SpeedexManagementStructures;

template<typename xdr_type>
bool sig_check(const xdr_type& data, const Signature& sig, const PublicKey& pk) {
	auto buf = xdr::xdr_to_opaque(data);

	return crypto_sign_verify_detached(
		sig.data(), buf.data(), buf.size(), pk.data()) == 0;
}

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
	gen_key_pair_list(size_t num_accounts);
};

} /* speedex */
