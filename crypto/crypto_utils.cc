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

#include "crypto/crypto_utils.h"

#include "speedex/speedex_management_structures.h"

#include <tbb/parallel_reduce.h>

#include <xdrpp/marshal.h>

#include <cstddef>

namespace speedex {

void 
initialize_crypto()
{
	if (sodium_init() == -1) {
		throw std::runtime_error("failed to init sodium");
	}
	std::printf("initialized sodium\n");
}

class SigCheckReduce {
	const SpeedexManagementStructures& management_structures;
	const SignedTransactionList& block;

public:

	bool valid = true;

	void operator() (const tbb::blocked_range<uint64_t> r) {
		if (!valid) return;

		bool temp_valid = true;

		for (uint64_t i = r.begin(); i < r.end(); i++) {
			auto sender_acct = block[i].transaction.metadata.sourceAccount;
			auto pk_opt =  management_structures.db.get_pk_nolock(sender_acct);
			if (!pk_opt) {

				std::printf("no pk! account %" PRIu64 "\n", sender_acct);
				temp_valid = false;
				break;
			}
			if (!sig_check(block[i].transaction, block[i].signature, *pk_opt)) {
				std::printf("tx %" PRIu64 "failed, %" PRIu64 "\n", i, sender_acct);
				temp_valid = false;
				break;
			}
		}
		valid = valid && temp_valid;
	}

	SigCheckReduce(
		const SpeedexManagementStructures& management_structures,
		const SignedTransactionList& block)
	: management_structures(management_structures)
	, block(block) {}

	SigCheckReduce(SigCheckReduce& other, tbb::split)
	: management_structures(other.management_structures)
	, block(other.block) {}

	void join(SigCheckReduce& other) {
		valid = valid && other.valid;
	}
};


bool 
BlockSignatureChecker::check_all_sigs(const SerializedBlock& block) {
	SignedTransactionList txs;
	xdr::xdr_from_opaque(block, txs);

	auto checker = SigCheckReduce(management_structures, txs);

	tbb::parallel_reduce(tbb::blocked_range<uint64_t>(0, txs.size(), 2000), checker);

	return checker.valid;
}

std::pair<std::vector<SecretKey>, std::vector<PublicKey>>
DeterministicKeyGenerator::gen_key_pair_list(size_t num_accounts) {
	std::vector<SecretKey> sks;
	std::vector<PublicKey> pks;
	sks.resize(num_accounts);
	pks.resize(num_accounts);
	tbb::parallel_for(
		tbb::blocked_range<size_t>(0, num_accounts),
		[this, &sks, &pks] (auto r) {
			for (auto i = r.begin(); i < r.end(); i++) {
				auto [sk, pk] = deterministic_key_gen(i);
				sks[i] = sk;
				pks[i] = pk;
			}
		});
	return std::make_pair(sks, pks);
}


// Clearly, a real-world system wouldn't generate keys all on the central server
std::pair<SecretKey, PublicKey> 
DeterministicKeyGenerator::deterministic_key_gen(uint64_t seed) {
	std::array<uint64_t, 4> seed_bytes; // 32 bytes
	seed_bytes.fill(0);
	seed_bytes[0] = seed;


	SecretKey sk;
	PublicKey pk;

	if (crypto_sign_seed_keypair(pk.data(), sk.data(), reinterpret_cast<unsigned char*>(seed_bytes.data()))) {
		throw std::runtime_error("sig gen failed!");
	}

	return std::make_pair(sk, pk);
}

void
sign_transaction(SignedTransaction& tx, SecretKey const& sk)
{
	auto msg = xdr::xdr_to_opaque(tx.transaction);
	if (crypto_sign_detached(
		tx.signature.data(), //signature
		nullptr, //optional siglen ptr
		msg.data(), //msg
		msg.size(), //msg len
		sk.data())) //sk
	{
		throw std::runtime_error("failed to sign!");
	}
}



} /* speedex */
