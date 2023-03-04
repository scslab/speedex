#pragma once

/**
 * SPEEDEX: A Scalable, Parallelizable, and Economically Efficient Decentralized Exchange
 * Copyright (C) 2023 Geoffrey Ramseyer

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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

void
sign_transaction(SignedTransaction& tx, SecretKey const& sk);

} /* speedex */
