#pragma once 

#include <cstdint>
#include <optional>
#include <random>
#include <unordered_map>
#include <unordered_set>

#include <tbb/parallel_for.h>
#include <xdrpp/marshal.h>

#include "config/replica_config.h"

#include "crypto/crypto_utils.h"

#include "speedex/speedex_options.h"

#include "synthetic_data_generator/synthetic_data_gen_options.h"

#include "utils/price.h"
#include "utils/save_load_xdr.h"
#include "utils/transaction_type_formatter.h"

#include "xdr/block.h"
#include "xdr/experiments.h"
#include "xdr/transaction.h"

namespace speedex {

class SyntheticDataGenSigner {
	DeterministicKeyGenerator key_gen;
	std::unordered_map<AccountID, SecretKey> key_map;

public:
	SyntheticDataGenSigner() 
		: key_gen() {}

	void add_account(AccountID new_account) {
		auto [sk, _] = key_gen.deterministic_key_gen(new_account);
		key_map[new_account] = sk;
	}

	PublicKey get_public_key(AccountID account) {
		auto [_, pk] = key_gen.deterministic_key_gen(account);
		return pk;
	}

	void sign_block(ExperimentBlock& txs);
};

struct BlockState {
	std::unordered_map<AccountID, uint64_t> sequence_num_map;

	std::vector<SignedTransaction> tx_buffer;
	std::vector<bool> cancel_flags;
	uint64_t block_number = 1;
};

template<typename random_generator>
struct GeneratorState {

	double bad_frac = 0;

	std::uniform_real_distribution<> type_dist;

	uint64_t num_active_accounts;

	bool is_payment_op(double tx_type_res) {
		return tx_type_res < options.payment_rate / (options.payment_rate + options.create_offer_rate + options.account_creation_rate);
	}

	bool is_create_offer_op(double tx_type_res) {
		double denom =  (options.payment_rate + options.create_offer_rate + options.account_creation_rate);
		return (tx_type_res >= options.payment_rate / denom) && (tx_type_res < (options.payment_rate + options.create_offer_rate)/ denom);
	}

	bool is_create_account_op(double tx_type_res) {
		double denom =  (options.payment_rate + options.create_offer_rate + options.account_creation_rate);
		return (tx_type_res >=  (options.payment_rate + options.create_offer_rate) / denom);
	}
	
	random_generator& gen;

	const GenerationOptions& options;

	SyntheticDataGenSigner signer;

	BlockState block_state;

	std::optional<std::pair<ReplicaID, ReplicaConfig>> conf_pair;

	std::string output_directory;

	xdr::xvector<AccountID> existing_accounts_map; // map from index [0, num_accounts) to accountID

	std::unordered_set<AccountID> existing_accounts_set;

	std::vector<double> asset_probabilities;
	std::uniform_real_distribution<> zero_one_dist = std::uniform_real_distribution<>(0, 1);


	void normalize_asset_probabilities();

	void add_account_mapping(uint64_t new_idx);
	AccountID allocate_new_account_id();

	OperationType gen_new_op_type();

	std::vector<std::vector<SignedTransaction>> cancel_txs;

	void add_cancel_tx(SignedTransaction tx) {
		auto delay_dist = std::uniform_int_distribution<>(options.cancel_delay_rounds_min, options.cancel_delay_rounds_max);

		size_t delay = delay_dist(gen);

		if (cancel_txs.size() < delay) {
			cancel_txs.resize(delay);
		}

		cancel_txs.at(delay-1).push_back(tx);
	}

	std::vector<SignedTransaction> 
	dump_current_round_cancel_txs() {
		if (cancel_txs.size() == 0) {
			return std::vector<SignedTransaction>();
		}
		auto out = cancel_txs.at(0);
		cancel_txs.erase(cancel_txs.begin());
		return out;
	}

	std::vector<double> gen_prices();
	void modify_prices(std::vector<double>& prices);
	void print_prices(const std::vector<double>& prices) const;

	std::vector<AssetID> gen_asset_cycle();
	int64_t gen_endowment(double price);
	AccountID gen_account();
	double gen_tolerance();
	double get_exact_price(const std::vector<double>& prices, const OfferCategory& category);
	double gen_good_price(double exact_price);
	double gen_bad_price(double exact_price);
	AssetID gen_asset();
	Operation make_sell_offer(int64_t amount, double ratio, const OfferCategory& category);

	std::vector<Operation>
	gen_good_offer_cycle(const std::vector<AssetID>& assets, const std::vector<double>& prices);
	//Does NOT fill in sequence numbers!  None of these tx gen methods do
	std::vector<SignedTransaction> 
	gen_good_tx_cycle(const std::vector<AssetID>& assets, const std::vector<double>& prices);
	Operation gen_bad_offer(const std::vector<double>& prices);
	SignedTransaction gen_bad_tx(const std::vector<double>& prices);
	SignedTransaction gen_account_creation_tx();
	SignedTransaction gen_payment_tx();
	SignedTransaction gen_cancel_tx(const SignedTransaction& creation_tx);

	bool good_offer_cancel();
	bool bad_offer_cancel();

	std::pair<
		std::vector<SignedTransaction>,
		std::vector<bool>
	>
	gen_transactions(size_t num_txs, const std::vector<double>& prices);

	// needs to be called in ctor to ensure rand gen always starts from consistent seed
	void gen_new_accounts(uint64_t num_new_accounts);

	void filter_by_replica_id(ExperimentBlock& block);

public:

	GeneratorState(
		random_generator& gen, 
		const GenerationOptions& options, 
		std::string output_directory, 
		std::optional<std::pair<ReplicaID, ReplicaConfig>> conf_pair = std::nullopt) 
		: type_dist(0.0, 1.0) 
		, num_active_accounts(0)
		, gen(gen)
		, options(options) 
		, signer()
		, conf_pair(conf_pair)
		, output_directory(output_directory) {
			gen_new_accounts(options.num_accounts);
		}

	xdr::xvector<Offer>
	make_offer_list(const std::vector<double>& prices, size_t num_offers);

	void make_offer_set(const std::vector<double>& prices);
	void make_block(const std::vector<double>& prices);

	void make_offer_sets();
	void make_blocks();

	void dump_account_list(std::string accounts_filename);

	xdr::xvector<AccountID> const& get_accounts()
	{
		return existing_accounts_map;
	}
};

}
