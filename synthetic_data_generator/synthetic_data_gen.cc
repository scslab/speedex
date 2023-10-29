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

#include "synthetic_data_gen.h"

#include <algorithm>
#include <cinttypes>

namespace speedex {

template class GeneratorState<std::minstd_rand>;

void
SyntheticDataGenSigner::sign_block(ExperimentBlock& txs) const {

	Signature s;
	if (s.size() != crypto_sign_BYTES) {
		throw std::runtime_error("sign len mismatch!!!");
	}

	tbb::parallel_for(
		tbb::blocked_range<size_t>(0, txs.size()),
		[&txs, this] (auto r) {


			for (auto i = r.begin(); i < r.end(); i++) {
				auto msg = xdr::xdr_to_opaque(txs[i].transaction);
				AccountID sender = txs[i].transaction.metadata.sourceAccount;
				if (crypto_sign_detached(
					txs[i].signature.data(), //signature
					nullptr, //optional siglen ptr
					msg.data(), //msg
					msg.size(), //msg len
					key_map.at(sender).data())) //sk
				{
					throw std::runtime_error("failed to sign!");
				}
			}
		});
}

template<typename random_generator>
void 
GeneratorState<random_generator>::gen_new_accounts(uint64_t num_new_accounts) {
	for (uint64_t i = 0; i < num_new_accounts; i++) {
		allocate_new_account_id();
	}
}

template<typename random_generator>
void
GeneratorState<random_generator>::dump_account_list(std::string accounts_filename) {
	AccountIDList list;
	list.reserve(num_active_accounts);

	for (AccountID acct : existing_accounts_set) {
		list.push_back(acct);
	}

	std::shuffle(list.begin(), list.end(), gen);

	if (save_xdr_to_file(list, accounts_filename.c_str())) {
		throw std::runtime_error("could not save accounts list!");
	}
}


template<typename random_generator>
std::vector<double> 
GeneratorState<random_generator>::gen_prices() {

	if (options.price_options.exp_param == 0) {
		std::uniform_real_distribution<> price_dist(options.price_options.min_price, options.price_options.max_price);
		std::vector<double> price_out;
		for (size_t i = 0; i < options.num_assets; i++) {
			price_out.push_back(price_dist(gen));
		}
		return price_out;
	} else {
		std::exponential_distribution<> price_dist(options.price_options.exp_param);
		std::vector<double> price_out;

		double range = options.price_options.max_price - options.price_options.min_price;
		for (size_t i = 0; i < options.num_assets; i++) {
			price_out.push_back(std::fmod(price_dist(gen), range) + options.price_options.min_price);
		}
		return price_out;
	}
}

template<typename random_generator>
void
GeneratorState<random_generator>::modify_prices(std::vector<double>& prices) {
	if (options.price_options.per_block_delta == 0.0) {
		return;
	}
	std::normal_distribution<> delta_dist(0, options.price_options.per_block_delta);

	for (size_t i = 0; i < options.num_assets; i++) {
		double delta = std::exp(delta_dist(gen)); // *= e^{normal(0, 1)}
		prices[i] *= delta;
	}

	/*std::uniform_real_distribution<> adjust_dist(0, options.price_options.per_block_delta * 2);
	for (size_t i = 0; i < options.num_assets; i++) {
		double delta = adjust_dist(gen) - options.price_options.per_block_delta;
		prices[i] *= (1.0 + delta);
	}*/
}

template<typename random_generator>
void
GeneratorState<random_generator>::print_prices(const std::vector<double>& prices) const {
	for (size_t i = 0; i < prices.size(); i++) {
		std::printf("asset %lu has valuation %lf\n", i, prices[i]);
	}
}


//not super efficient but ah well
template<typename random_generator>
std::vector<AssetID> 
GeneratorState<random_generator>::gen_asset_cycle() {

	std::uniform_real_distribution<> size_rand (0.0, 1.0);
	size_t size = options.cycle_dist.get_size(size_rand(gen));

	if (options.reserve_currency) {
		AssetID candidate = 0;
		while (candidate == 0) {
			candidate = gen_asset();
		}
		return {0, candidate};
	}

	//if (options.asset_bias != 0) {
		std::vector<AssetID> output;
		//std::printf("gen cycle size %lu\n", size);
		for (size_t i = 0; i < size; i++) {
			while(true) {
				AssetID candidate = gen_asset();
				if (std::find(output.begin(), output.end(), candidate) == output.end()) {
					output.push_back(candidate);
					break;
				}
			}
		}
	//std::printf("done\n");
		return output;
	//}

	//std::vector<AssetID> output;
	//for (size_t i = 0; i < options.num_assets; i++) {
	//	output.push_back(i);
	//}

	//std::shuffle(output.begin(), output.end(), gen);

	//return std::vector(output.begin(), output.begin() + size);
}
template<typename random_generator>
size_t 
GeneratorState<random_generator>::gen_random_index(size_t length)
{
	std::uniform_int_distribution<> size_dist(0, length);
	return size_dist(gen);
}


template<typename random_generator>
int64_t 
GeneratorState<random_generator>::gen_endowment(double price) {
	std::uniform_int_distribution<> endow_dist(options.initial_endow_min, options.initial_endow_max);
	std::uniform_real_distribution<> whale_dist(0, 1);

	int64_t amount = endow_dist(gen);
	if (amount > 1000000) {
		throw std::runtime_error("invalid endow amount!");
	}

	if (options.normalize_values) {
		amount = (100 * amount ) / price;
	}

	if (whale_dist(gen) < options.whale_percentage) {
		amount *= options.whale_multiplier;
	}

	return amount;
}


template<typename random_generator>
AccountID 
GeneratorState<random_generator>::gen_account() {
	if (options.account_dist_param == 0) {
		std::uniform_int_distribution<> account_dist(0, num_active_accounts - 1);
		uint64_t idx = account_dist(gen);
		return existing_accounts_map.at(idx);
	}

	std::exponential_distribution<> account_dist(options.account_dist_param);

	uint64_t account_idx = static_cast<uint64_t>(std::floor(account_dist(gen))) % num_active_accounts;
	return existing_accounts_map.at(account_idx);
}

template<typename random_generator>
OperationType
GeneratorState<random_generator>::gen_new_op_type() {
	double tx_type_res = type_dist(gen);

	if (is_payment_op(tx_type_res)) {
		return OperationType::PAYMENT;
	}
	if (is_create_offer_op(tx_type_res)) {
		return OperationType::CREATE_SELL_OFFER;
	}
	if (is_create_account_op(tx_type_res)) {
		return OperationType::CREATE_ACCOUNT;
	}
	std::printf("invalid tx_type_res %lf\n", tx_type_res);
	throw std::runtime_error("invalid tx_type_res");
}

template<typename random_generator>
void 
GeneratorState<random_generator>::add_account_mapping(uint64_t new_idx) {
	std::uniform_int_distribution<uint64_t> account_gen_dist(0, UINT64_MAX);
	while(true) {
		AccountID new_acct = account_gen_dist(gen);
		if (existing_accounts_set.find(new_acct) != existing_accounts_set.end()) {
			continue;
		}

		existing_accounts_set.insert(new_acct);
		if (existing_accounts_map.size() != new_idx) {
			throw std::runtime_error("bad usage of add_account_mapping");
		}
		existing_accounts_map.push_back(new_acct);
		return;
	}
}

template<typename random_generator>
AccountID 
GeneratorState<random_generator>::allocate_new_account_id() {
	uint64_t new_idx = num_active_accounts;
	add_account_mapping(new_idx);

	AccountID out = existing_accounts_map[new_idx];
	num_active_accounts ++;
	signer.add_account(out);
	return out;
}

template<typename random_generator>
double 
GeneratorState<random_generator>::gen_tolerance() {
	std::uniform_real_distribution<> tolerance_dist(options.price_options.min_tolerance, options.price_options.max_tolerance);
	return tolerance_dist(gen);
}

template<typename random_generator>
double 
GeneratorState<random_generator>::get_exact_price(const std::vector<double>& prices, const OfferCategory& category) {
	return prices.at(category.sellAsset) / prices.at(category.buyAsset);
}

template<typename random_generator>
Operation 
GeneratorState<random_generator>::make_sell_offer(int64_t amount, double ratio, const OfferCategory& category) {
	Price min_price = price::from_double(ratio);
	CreateSellOfferOp op {category, amount, min_price};
	return tx_formatter::make_operation(op);
}

Offer
sell_offer_op_to_offer(Operation op, AccountID owner, uint64_t& seqNum) {
	Offer out;
	out.category = op.body.createSellOfferOp().category;
	out.amount = op.body.createSellOfferOp().amount;
	out.minPrice = op.body.createSellOfferOp().minPrice;
	out.owner = owner;
	out.offerId = seqNum;
	seqNum++;
	return out;
}

template<typename random_generator>
double 
GeneratorState<random_generator>::gen_good_price(double exact_price) {
	return exact_price * (1.0 - gen_tolerance());
}

template<typename random_generator>
double 
GeneratorState<random_generator>::gen_bad_price(double exact_price) {
	return exact_price + (1.0 + gen_tolerance());
}

template<typename random_generator>
std::vector<Operation>
GeneratorState<random_generator>::gen_good_offer_cycle(const std::vector<AssetID>& assets, const std::vector<double>& prices) {
	std::vector<Operation> output;

	int64_t endow = gen_endowment(prices[assets[0]]);

	std::vector<std::pair<double, int64_t>> prev_endows;

	for (unsigned int i = 0; i < assets.size(); i++) {
		OfferCategory category(assets.at(i), assets.at((i+1)%assets.size()), OfferType::SELL);
		if (category.buyAsset == category.sellAsset) {
			throw std::runtime_error("shouldn't have identical buy and sell assets");
		}
		double min_price = get_exact_price(prices, category);

		auto offer = make_sell_offer(endow, gen_good_price(min_price), category);
		output.push_back(offer);
	}
	return output;
}


//does not fill in seq numbers
template<typename random_generator>
std::vector<SignedTransaction> 
GeneratorState<random_generator>::gen_good_tx_cycle(const std::vector<AssetID>& assets, const std::vector<double>& prices) {
	
	std::vector<SignedTransaction> output;

	int64_t endow = gen_endowment(prices[assets[0]]);
	//int64_t start_endow = endow;

	std::vector<std::pair<double, int64_t>> prev_endows;

//	bool error_flag = false;
	for (unsigned int i = 0; i < assets.size(); i++) {
		OfferCategory category(assets.at(i), assets.at((i+1)%assets.size()), OfferType::SELL);
		if (category.buyAsset == category.sellAsset) {
			throw std::runtime_error("shouldn't have identical buy and sell assets");
		}
		double min_price = get_exact_price(prices, category);

		SignedTransaction tx;
		tx.transaction.operations.push_back(make_sell_offer(endow, gen_good_price(min_price), category));
		endow = std::ceil(endow * min_price);

		prev_endows.emplace_back(min_price, endow);

		tx.transaction.metadata.sourceAccount = gen_account();
		output.push_back(tx);
		//if (endow >= 20000000) {
		//	std::printf("invalid amount! %lu %lf\n", endow, min_price);
		//	error_flag = true;
		//}
	}

	/*if (endow > 1000 * start_endow || error_flag) {

		for (auto& pair : prev_endows) {
			std::printf("endow: %lu after mult by price %lf\n", pair.second, pair.first);
		}

		std::printf("start_endow: %ld endow: %ld\n", start_endow, endow);

		int64_t recreate_endow = start_endow;
		for (size_t i = 0; i < assets.size(); i++) {
			OfferCategory category(assets.at(i), assets.at((i+1)%assets.size()), OfferType::SELL);
			double min_price = get_exact_price(prices, category);

			recreate_endow = std::ceil(recreate_endow * min_price);
			std::printf("sell %lf buy %lf\n", prices.at(assets.at(i)), prices.at(assets.at((i+1)% assets.size())));
			std::printf("min price: %lf recreate_endow: %ld\n", min_price, recreate_endow);
		}
		
		throw std::runtime_error("some kind of miscomputation in endow calculation!");
	}*/
	return output;
}

template<typename random_generator>
void
GeneratorState<random_generator>::normalize_asset_probabilities() {
	if (asset_probabilities.size() == 0) {
		return;
	}

	//double sum = 0;
	//for (size_t i = 0; i < asset_probabilities.size(); i++) {
	//	sum += asset_probabilities[i];
	//	std::printf("%lu %lf\n", i, asset_probabilities[i]);
	//}
	double sum = asset_probabilities[asset_probabilities.size() - 1];
	std::printf("asset probs:\n");
	for (size_t i = 0; i < asset_probabilities.size(); i++) {
		asset_probabilities[i] /= sum;
		std::printf("%lu %lf\n",i, asset_probabilities[i]);
	}
	asset_probabilities[asset_probabilities.size() - 1] = 1;
}

template<typename random_generator>
AssetID 
GeneratorState<random_generator>::gen_asset() {
	if (asset_probabilities.size() > 0) {
		double amt = zero_one_dist(gen);

		for (size_t i = 0; i < asset_probabilities.size(); i++) {
			if (amt <= asset_probabilities[i]) {
				return i;
			}
		}
		throw std::runtime_error("invalid asset_probabilities!");
	}

	if (options.asset_bias == 0) {
		std::uniform_int_distribution<> asset_dist(0, options.num_assets - 1);
		return asset_dist(gen);
	}

	else {
		std::exponential_distribution<> asset_dist(options.asset_bias);
		return (static_cast<AssetID>(std::floor(asset_dist(gen)))) % options.num_assets;
	}
}

template<typename random_generator>
Operation
GeneratorState<random_generator>::gen_bad_offer(const std::vector<double>& prices) {
	OfferCategory category;
	category.type = OfferType::SELL;
	category.sellAsset = gen_asset();
	category.buyAsset = category.sellAsset;
	while (category.sellAsset == category.buyAsset) {
		category.buyAsset = gen_asset();
	}

	double exact_price = get_exact_price(prices, category);
	double bad_price = gen_bad_price(exact_price);

	return make_sell_offer(gen_endowment(prices[category.sellAsset]), bad_price, category);
}

template<typename random_generator>
SignedTransaction 
GeneratorState<random_generator>::gen_bad_tx(const std::vector<double>& prices) {
	OfferCategory category;
	category.type = OfferType::SELL;
	category.sellAsset = gen_asset();
	category.buyAsset = category.sellAsset;
	while (category.sellAsset == category.buyAsset) {
		category.buyAsset = gen_asset();
	}

	double exact_price = get_exact_price(prices, category);
	double bad_price = gen_bad_price(exact_price);

	SignedTransaction tx;
	tx.transaction.operations.push_back(make_sell_offer(gen_endowment(prices[category.sellAsset]), bad_price, category));
	tx.transaction.metadata.sourceAccount = gen_account();
	return tx;
}

template<typename random_generator>
SignedTransaction
GeneratorState<random_generator>::gen_account_creation_tx() {

	AccountID new_account_id = allocate_new_account_id();

	SignedTransaction tx;
	tx.transaction.metadata.sourceAccount = gen_account();

	CreateAccountOp create_op;

	create_op.startingBalance = CREATE_ACCOUNT_MIN_STARTING_BALANCE;
	create_op.newAccountId = new_account_id;
	create_op.newAccountPublicKey = signer.get_public_key(new_account_id);

	tx.transaction.operations.push_back(tx_formatter::make_operation(create_op));

	for (AssetID i = 0; i < options.num_assets; i++) {
		MoneyPrinterOp money_printer_op;
		money_printer_op.asset = i;
		money_printer_op.amount = options.new_account_balance;
		tx.transaction.operations.push_back(tx_formatter::make_operation(money_printer_op));

		PaymentOp payment_op;
		payment_op.asset = i;
		payment_op.receiver = new_account_id;
		payment_op.amount = options.new_account_balance;
		tx.transaction.operations.push_back(tx_formatter::make_operation(payment_op));
	}
	return tx;
}

template<typename random_generator>
SignedTransaction 
GeneratorState<random_generator>::gen_payment_tx(int64_t amount) {
	AccountID sender = gen_account();
	AccountID receiver = gen_account();
	AssetID asset = gen_asset();
	if (amount < 0) {
		amount = gen_endowment(1);
	}
	PaymentOp op;
	op.receiver = receiver;
	op.asset = asset;
	op.amount = amount;
	SignedTransaction tx;
	tx.transaction.operations.push_back(tx_formatter::make_operation(op));

	tx.transaction.metadata.sourceAccount = sender;
	
	return tx;
}

template<typename random_generator>
SignedTransaction
GeneratorState<random_generator>::gen_cancel_tx(const SignedTransaction& creation_tx) {
	const CreateSellOfferOp& creation_op = creation_tx.transaction.operations.at(0).body.createSellOfferOp();

	CancelSellOfferOp cancel_op;
	cancel_op.category = creation_op.category;
	cancel_op.offerId = creation_tx.transaction.metadata.sequenceNumber; // points to first operation within the transaction
	cancel_op.minPrice = creation_op.minPrice;

	SignedTransaction tx_out;
	tx_out.transaction.metadata.sourceAccount = creation_tx.transaction.metadata.sourceAccount;
	tx_out.transaction.operations.push_back(tx_formatter::make_operation(cancel_op));
	return tx_out;
}

template<typename random_generator>
bool 
GeneratorState<random_generator>::bad_offer_cancel() {
	std::uniform_real_distribution<> dist(0, 1);
	return dist(gen) < options.bad_offer_cancel_chance;
}

template<typename random_generator>
bool 
GeneratorState<random_generator>::good_offer_cancel() {
	std::uniform_real_distribution<> dist(0, 1);
	return dist(gen) < options.good_offer_cancel_chance;
}

bool is_cancellable(const SignedTransaction& tx) {
	if (tx.transaction.operations.at(0).body.type() == CREATE_SELL_OFFER) {
		return true;
	}
	return false;
}

template<typename random_generator>
std::pair<
		std::vector<SignedTransaction>,
		std::vector<bool>
	>
GeneratorState<random_generator>::gen_transactions(size_t num_txs, const std::vector<double>& prices) {

	std::vector<SignedTransaction> output;
	std::vector<bool> cancellation_flags;

	cancellation_flags.resize(num_txs, false);

	int print_freq = 100000;
	int print_count = 0;
	while (output.size() < num_txs) {
		switch(gen_new_op_type()) {
			case OperationType::CREATE_SELL_OFFER:
				if (bad_frac > 1) {
					bad_frac -= 1.0;
					output.push_back(gen_bad_tx(prices));
					cancellation_flags.at(output.size() - 1) = bad_offer_cancel();
					print_count++;
				} else {
					auto asset_cycle = gen_asset_cycle();
					print_count += asset_cycle.size();
					bad_frac += asset_cycle.size() * options.bad_tx_fraction;
					auto tx_cycle = gen_good_tx_cycle(asset_cycle, prices);

					for (size_t i = output.size(); i < output.size() + tx_cycle.size(); i++) {
						if (i >= cancellation_flags.size()) {
							cancellation_flags.resize(output.size() + tx_cycle.size());
						}
						cancellation_flags.at(i) = good_offer_cancel();
					}
					output.insert(output.end(), tx_cycle.begin(), tx_cycle.end());
				}
				break;
			case OperationType::PAYMENT:
				
				output.push_back(gen_payment_tx());
				print_count++;
				break;

			case OperationType::CREATE_ACCOUNT:
				output.push_back(gen_account_creation_tx());
				print_count ++;
				break;

			default:
				throw std::runtime_error("invalid op type!!!");
		}

		if (print_count > print_freq) {
			std::printf("%lu\n", output.size());
			print_count -= print_freq;	
		}
	}
	return std::make_pair(output, cancellation_flags);
}

template<typename random_generator>
void
GeneratorState<random_generator>::filter_by_replica_id(ExperimentBlock& block) {

	if (conf_pair) {

		for (size_t i = 0; i < block.size();) {
			AccountID src_account = block[i].transaction.metadata.sourceAccount;
			if (src_account % (conf_pair -> second->nreplicas) != conf_pair -> first) {
				block[i] = block.back();
				block.pop_back();
			} else {
				++i;
			}
		}
	}
}

template<typename random_generator>
void
GeneratorState<random_generator>::fill_in_seqnos(ExperimentBlock& block)
{
	for (unsigned int i = 0; i < block.size(); i++) {
		uint64_t prev_seq_num = block_state.sequence_num_map[block[i].transaction.metadata.sourceAccount];
		block[i].transaction.metadata.sequenceNumber = (prev_seq_num + 1) << 8;
		block[i].transaction.maxFee = tx_formatter::compute_min_fee(block[i]);// BASE_FEE_PER_TX + FEE_PER_OP * block[i].transaction.operations.size();
		block_state.sequence_num_map[block.at(i).transaction.metadata.sourceAccount] ++;
	}
}

template<typename random_generator>
ExperimentBlock GeneratorState<random_generator>::make_block(const std::vector<double>& prices) {
	normalize_asset_probabilities();

	if (2 * options.block_size < block_state.tx_buffer.size()) {
		std::printf("previous round overfilled block buffer.\n");

	} else {
		size_t new_txs_count = 2 * options.block_size - block_state.tx_buffer.size();

		auto [new_txs, new_cancellation_flags] = gen_transactions(new_txs_count, prices);
		block_state.tx_buffer.insert(block_state.tx_buffer.end(), new_txs.begin(), new_txs.end());
		block_state.cancel_flags.insert(block_state.cancel_flags.end(), new_cancellation_flags.begin(), new_cancellation_flags.end());
	}
		
	std::uniform_int_distribution<> idx_dist (0, options.block_size - 1);

	int num_swaps = options.block_size * options.block_boundary_crossing_fraction;

	for (int swap_cnt = 0; swap_cnt < num_swaps; swap_cnt++) {
		int idx_1 = idx_dist(gen);
		int idx_2 = idx_dist(gen) + options.block_size; 

		std::swap(block_state.tx_buffer[idx_1], block_state.tx_buffer[idx_2]);
		// This used to work with just regular swap(), but vector<bool> is unusual,
		// and gcc13.{1,2} doesn't like swap() on vector<bool>
		std::iter_swap(block_state.cancel_flags.begin() + idx_1, block_state.cancel_flags.begin() + idx_2);
	}

	auto& txs = block_state.tx_buffer;
	auto& cancellation_flags = block_state.cancel_flags;

	ExperimentBlock output;
	output.insert(output.end(), txs.begin(), txs.begin() + options.block_size);

	fill_in_seqnos(output);

	for (unsigned int i = 0; i < options.block_size; i++) {
		//uint64_t prev_seq_num = block_state.sequence_num_map[txs[i].transaction.metadata.sourceAccount];
		//txs[i].transaction.metadata.sequenceNumber = (prev_seq_num + 1) << 8;
		//block_state.sequence_num_map[txs.at(i).transaction.metadata.sourceAccount] ++;

		if (cancellation_flags.at(i)) {
			add_cancel_tx(gen_cancel_tx(output.at(i)));
		}
	}

	filter_by_replica_id(output);
	
	//std::string filename = output_directory + std::to_string(block_state.block_number) + ".txs";
	//block_state.block_number ++;


	if (options.do_shuffle) {
		shuffle_block(output);
		//std::shuffle(output.begin(), output.end(), gen);
	}

	signer.sign_block(output);

	//xdr::opaque_vec<> serialized_output = xdr::xdr_to_opaque(output);

//	if (save_xdr_to_file(serialized_output, filename.c_str())) {
//		throw std::runtime_error("was not able to save file!");
//	}

	write_block(output);


	txs.erase(txs.begin(), txs.begin() + options.block_size);
	cancellation_flags.erase(cancellation_flags.begin(), cancellation_flags.begin() + options.block_size);

	auto cancel_txs = dump_current_round_cancel_txs();

	txs.insert(txs.end(), cancel_txs.begin(), cancel_txs.end());
	cancellation_flags.resize(txs.size(), false);

	return output;
}

template<typename random_generator>
void
GeneratorState<random_generator>::write_block(ExperimentBlock& block)
{
	std::printf("writing block %" PRIu64 "\n", block_state.block_number);
	std::string filename = output_directory + std::to_string(block_state.block_number) + ".txs";
	block_state.block_number ++;
	xdr::opaque_vec<> serialized_output = xdr::xdr_to_opaque(block);

	if (save_xdr_to_file(serialized_output, filename.c_str())) {
		std::printf("failed to save file %s\n", filename.c_str());
		std::fflush(stdout);
		throw std::runtime_error("was not able to save file!");
	}
}

template<typename random_generator>
void 
GeneratorState<random_generator>::shuffle_block(ExperimentBlock& block)
{
	std::shuffle(block.begin(), block.end(), gen);
}

template<typename random_generator>
void
GeneratorState<random_generator>::shuffle_offers_and_fill_in_seqnos(xdr::xvector<Offer>& offers)
{
	std::shuffle(offers.begin(), offers.end(), gen);

	for (auto& offer : offers)
	{
		AccountID owner = offer.owner;
		uint64_t prev_seq_num = block_state.sequence_num_map[owner];
		offer.offerId = ((prev_seq_num + 1) << 8);
		block_state.sequence_num_map[owner] ++;
	}
}

template<typename random_generator>
xdr::xvector<Offer>
GeneratorState<random_generator>::make_offer_list(const std::vector<double>& prices, size_t num_offers)
{
	normalize_asset_probabilities();
	
	xdr::xvector<Offer> offers;
	uint64_t seqNum = 0;
	double bad_frac = 0;

	while(offers.size() < num_offers) {
		if (bad_frac > 1) {
			bad_frac -= 1.0;
			AccountID acct = gen_account();
			offers.push_back(sell_offer_op_to_offer(gen_bad_offer(prices), acct, seqNum));
		} else {
			auto asset_cycle = gen_asset_cycle();
			bad_frac += asset_cycle.size() * options.bad_tx_fraction;
			auto offer_cycle = gen_good_offer_cycle(asset_cycle, prices);

			for (auto offer : offer_cycle) {

				offers.push_back(sell_offer_op_to_offer(offer, gen_account(), seqNum));
			}
		}
	}
	shuffle_offers_and_fill_in_seqnos(offers);
	return offers;
}

template<typename random_generator>
void GeneratorState<random_generator>::make_offer_set(const std::vector<double>& prices) {
	normalize_asset_probabilities();

	TatonnementExperimentData sim_data;

	sim_data.offers = make_offer_list(prices, options.block_size);

	std::string filename = output_directory + std::to_string(block_state.block_number) + ".offers";
	block_state.block_number ++;

	sim_data.num_assets = options.num_assets;

	std::printf("made %lu offers\n", sim_data.offers.size());

	if (save_xdr_to_file(sim_data, filename.c_str())) {
		throw std::runtime_error(std::string("was not able to save file = ") + filename);
	}
}


template<typename random_generator>
void 
GeneratorState<random_generator>::make_blocks() {
	std::vector<double> prices = gen_prices();

	for (size_t i = 0; i < options.num_blocks; i++) {
		make_block(prices);
		modify_prices(prices);
		print_prices(prices);
	}
}

template<typename random_generator>
void
GeneratorState<random_generator>::make_offer_sets() {
	std::vector<double> prices = gen_prices();

	for (size_t i = 0; i < options.num_blocks; i++) {
		make_offer_set(prices);
		modify_prices(prices);
		print_prices(prices);
	}
}

} /* speedex */


