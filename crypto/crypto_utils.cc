#include "crypto/crypto_utils.h"

#include "speedex/speedex_management_structures.h"

#include <tbb/parallel_reduce.h>

#include <cstddef>

namespace speedex {

class SigCheckReduce {
	const SpeedexManagementStructures& management_structures;
	const SignedTransactionList& block;

public:

	bool valid = true;

	void operator() (const tbb::blocked_range<size_t> r) {
		if (!valid) return;

		bool temp_valid = true;

		for (size_t i = r.begin(); i < r.end(); i++) {
			auto sender_acct = block[i].transaction.metadata.sourceAccount;
			auto pk_opt =  management_structures.db.get_pk_nolock(sender_acct);
			if (!pk_opt) {

				std::printf("no pk! account %lu\n", sender_acct);
				temp_valid = false;
				break;
			}
			if (!sig_check(block[i].transaction, block[i].signature, *pk_opt)) {
				std::printf("tx %lu failed, %lu\n", i, sender_acct);
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

	tbb::parallel_reduce(tbb::blocked_range<size_t>(0, txs.size(), 2000), checker);

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



} /* speedex */
