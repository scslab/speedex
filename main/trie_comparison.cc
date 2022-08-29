

#include <unordered_set>
#include <set>
#include <unordered_map>
#include <map>
#include <cstdint>
#include <random>

#include "orderbook/typedefs.h"

#include "modlog/log_entry_fns.h"
#include "modlog/typedefs.h"

#include "mtt/trie/merkle_trie.h"

#include "mtt/trie/recycling_impl/trie.h"

#include "utils/price.h"
#include "mtt/utils/time.h"

#include "xdr/types.h"
#include "xdr/database_commitments.h"

#include <sodium.h>

using namespace speedex;

using utils::init_time_measurement;
using utils::measure_time;

std::vector<AccountID> make_accounts(size_t num_accounts) {
	std::minstd_rand gen(0);
	std::uniform_int_distribution<uint64_t> account_gen_dist(0, UINT64_MAX);


	std::vector<AccountID> accounts;
	accounts.reserve(num_accounts);
	for (size_t i = 0; i < num_accounts; i++) {
		accounts.push_back(account_gen_dist(gen));
	}
	return accounts;
}

float test_std_set(const std::vector<AccountID>& accounts) {
	std::set<AccountID> set;
	auto timestamp = init_time_measurement();
	for (auto& account : accounts) {
		set.insert(account);
	}
	auto res = measure_time(timestamp);
	if (set.size() != accounts.size()) {
		throw std::runtime_error("size mismatch!");
	}
	return res;
}

float test_std_unordered_set(const std::vector<AccountID>& accounts) {
	std::unordered_set<AccountID> set;
	auto timestamp = init_time_measurement();
	for (auto& account : accounts) {
		set.insert(account);
	}
	auto res = measure_time(timestamp);
	if (set.size() != accounts.size()) {
		throw std::runtime_error("size mismatch!");
	}
	return res;
}

float test_std_map_emptyvalue(const std::vector<AccountID>& accounts) {
	std::map<AccountID, trie::EmptyValue> set;
	auto timestamp = init_time_measurement();
	for (auto& account : accounts) {
		set.insert({account, trie::EmptyValue{}});
	}
	auto res = measure_time(timestamp);
	if (set.size() != accounts.size()) {
		throw std::runtime_error("size mismatch!");
	}
	return res;
}

float test_std_unordered_map_emptyvalue(const std::vector<AccountID>& accounts) {
	std::unordered_map<AccountID, trie::EmptyValue> set;
	auto timestamp = init_time_measurement();
	for (auto& account : accounts) {
		set.insert({account, trie::EmptyValue{}});
	}
	auto res = measure_time(timestamp);
	if (set.size() != accounts.size()) {
		throw std::runtime_error("size mismatch!");
	}
	return res;
}

float test_smallnode_trie_emptyvalue(const std::vector<AccountID>& accounts) {
	trie::RecyclingTrie<trie::EmptyValue> trie;
	auto serial_trie = trie.open_serial_subsidiary();
	auto timestamp = init_time_measurement();
	for (auto& account : accounts) {
		serial_trie.insert(account, trie::EmptyValue{});
	}
	trie.merge_in(serial_trie);
	auto res = measure_time(timestamp);
	if (trie.size() != accounts.size()) {
		throw std::runtime_error("size mismatch!");
	}
	return res; 
}

using ValueT = AccountModificationTxListWrapper;
using StaticValueT = OfferWrapper;


float test_smallnode_trie_emptyvalue_reuse(const std::vector<AccountID>& accounts, trie::RecyclingTrie<trie::EmptyValue>& trie) {
	auto serial_trie = trie.open_serial_subsidiary();
	auto timestamp = init_time_measurement();
	for (auto& account : accounts) {
		serial_trie.insert(account, trie::EmptyValue{});
	}
	trie.merge_in(serial_trie);
	auto res = measure_time(timestamp);
	if (trie.size() != accounts.size()) {
		throw std::runtime_error("size mismatch");
	}
	return res;
}

float test_smallnode_trie_reuse(const std::vector<AccountID>& accounts, trie::RecyclingTrie<ValueT>& trie) {
	auto serial_trie = trie.open_serial_subsidiary();
	ValueT value_buffer;
	value_buffer.identifiers_self.push_back(27);
	value_buffer.identifiers_others.resize(1);
	auto timestamp = init_time_measurement();
	for (auto& account : accounts) {
		value_buffer.owner = account;
		value_buffer.identifiers_others[0] = TxIdentifier{account + 1, 23};
		serial_trie.insert(account, ValueT(value_buffer));
	}
	trie.merge_in(serial_trie);
	auto res = measure_time(timestamp);
	if (trie.size() != accounts.size()) {
		throw std::runtime_error("size mismatch");
	}
	return res;
} 

float test_smallnode_trie_reuse_offer(const std::vector<AccountID>& accounts, trie::RecyclingTrie<StaticValueT>& trie) {
	auto serial_trie = trie.open_serial_subsidiary();
	//serial_trie.print_offsets();

	StaticValueT value_buffer;
	value_buffer.amount = 1000;

	auto timestamp = init_time_measurement();
	for (auto& account : accounts) {

		//value_buffer.owner = account;
		//value_buffer.identifiers_others[0] = TxIdentifier{account + 1, 23};
		value_buffer.owner = account;
		serial_trie.insert(account, StaticValueT(value_buffer));
	}
	trie.merge_in(serial_trie);
	auto res = measure_time(timestamp);
	if (trie.size() != accounts.size()) {
		throw std::runtime_error("size mismatch");
	}
	return res;
}

float test_smallnode_trie_reuse_offer_hash(const std::vector<AccountID>& accounts, trie::RecyclingTrie<StaticValueT>& trie) {
	auto serial_trie = trie.open_serial_subsidiary();
	//serial_trie.print_offsets();
	StaticValueT value_buffer;
	value_buffer.amount = 1000;
	for (auto& account : accounts) {
		//value_buffer.owner = account;
		//value_buffer.identifiers_others[0] = TxIdentifier{account + 1, 23};
		value_buffer.owner = account;
		serial_trie.insert(account, StaticValueT(value_buffer));
	}
	trie.merge_in(serial_trie);
	if (trie.size() != accounts.size()) {
		throw std::runtime_error("size mismatch");
	}

	auto timestamp = init_time_measurement();
	Hash hash;
	trie.hash(hash);

	auto res = measure_time(timestamp);

	return res;
}


float test_smallnode_trie_reuse_txlog_hash(const std::vector<AccountID>& accounts, trie::RecyclingTrie<ValueT>& trie) {
	auto serial_trie = trie.open_serial_subsidiary();
	//serial_trie.print_offsets();
	ValueT value_buffer;
	value_buffer.identifiers_self.push_back(27);
	value_buffer.identifiers_others.resize(1);
	for (auto& account : accounts) {
		//value_buffer.owner = account;
		//value_buffer.identifiers_others[0] = TxIdentifier{account + 1, 23};
		value_buffer.owner = account;
		value_buffer.identifiers_others[0] = TxIdentifier{account+1, 23};
		serial_trie.insert(account, ValueT(value_buffer));
	}
	trie.merge_in(serial_trie);
	if (trie.size() != accounts.size()) {
		throw std::runtime_error("size mismatch");
	}

	auto timestamp = init_time_measurement();
	Hash hash;
	trie.hash<LogNormalizeFn>(hash);

	auto res = measure_time(timestamp);

	return res;
} 




float test_std_map(const std::vector<AccountID>& accounts) {
	std::map<AccountID, ValueT> map;
	ValueT value_buffer;
	value_buffer.identifiers_self.push_back(27);
	value_buffer.identifiers_others.resize(1);
	auto timestamp = init_time_measurement();
	for (auto& account : accounts) {
		value_buffer.owner = account;
		value_buffer.identifiers_others[0] = TxIdentifier{account + 1, 23};
		map.emplace(account, ValueT(value_buffer));
	}
	auto res = measure_time(timestamp);
	if (map.size() != accounts.size()) {
		throw std::runtime_error("sz mismatch");
	}
	return res;
}

float test_std_unordered_map(const std::vector<AccountID>& accounts) {
	std::unordered_map<AccountID, ValueT> map;
	ValueT value_buffer;
	value_buffer.identifiers_self.push_back(27);
	value_buffer.identifiers_others.resize(1);
	auto timestamp = init_time_measurement();
	for (auto& account : accounts) {
		value_buffer.owner = account;
		value_buffer.identifiers_others[0] = TxIdentifier{account + 1, 23};
		map.emplace(account, ValueT(value_buffer));
	}
	auto res = measure_time(timestamp);
	if (map.size() != accounts.size()) {
		throw std::runtime_error("sz mismatch");
	}
	return res;
}

float test_merkle_trie_emptyvalue(const std::vector<AccountID>& accounts) {
	using LogMetadataT = trie::CombinedMetadata<trie::SizeMixin>;
	using prefix_t = trie::ByteArrayPrefix<8>;
	using TrieT = trie::MerkleTrie<prefix_t, trie::EmptyValue, LogMetadataT>;
	auto timestamp = init_time_measurement();
	prefix_t prefix_buffer;
	TrieT trie;
	for (auto& account : accounts) {
		utils::write_unsigned_big_endian(prefix_buffer, account);
		trie.insert(prefix_buffer);
	}
	auto res = measure_time(timestamp);
	if (trie.size() != accounts.size()) {
		throw std::runtime_error("size mismatch!");
	}
	return res;
}

float test_merkle_trie(const std::vector<AccountID>& accounts) {
	using LogMetadataT = trie::CombinedMetadata<trie::SizeMixin>;
	using prefix_t = trie::ByteArrayPrefix<8>;
	using TrieT = trie::MerkleTrie<prefix_t, ValueT, LogMetadataT>;
	auto timestamp = init_time_measurement();
	prefix_t prefix_buffer;
	ValueT value_buffer;
	value_buffer.identifiers_self.push_back(27);
	value_buffer.identifiers_others.resize(1);
	TrieT trie;
	for (auto& account : accounts) {
		utils::write_unsigned_big_endian(prefix_buffer, account);
		value_buffer.owner = account;
		value_buffer.identifiers_others[0] = TxIdentifier{account + 1, 23};
		trie.insert(prefix_buffer, ValueT(value_buffer));
	}
	auto res = measure_time(timestamp);
	if (trie.size() != accounts.size()) {
		throw std::runtime_error("size mismatch!");
	}
	return res;
}

float test_account_log(const std::vector<AccountID>& accounts) {
	return 0;
}


int main(int argc, char const *argv[])
{
	if (argc != 3) {
		std::printf("usage: ./blah <test number> <num_accounts>\n");
		std::printf("test_number:\n0 std::set\n1 std::unordered_set\n2 std::map emptyvalue\n3 std::unordered_map emptyvalue\n4 smallnode_trie_emptyvalue\n5 merkle_trie_emptyvalue\n");
		std::printf("6 smallnode_trie_emptyvalue reusing memory buffer\n7 smallnode_trie_value reuse buffer (value = txmodlist)\n");
		std::printf("8 std::map with value (txmodlist)\n9 std::unordered_map with value\n10 smallnode_trie_value reuse buffer (value = offer\n");
		std::printf("11 merkle_trie value (txmodlist)\n12 HASH smallnode_trie reuse buffer (value=offer)\n");
		std::printf("13 HASH smallnode_trie reuse  buffer (value = txmodlist)\n");
		return 1;
	}

	size_t num_accounts = std::stoi(argv[2]);
	size_t test = std::stoi(argv[1]);
	auto accounts = make_accounts(num_accounts);

	trie::RecyclingTrie<trie::EmptyValue> reuse_trie;
	trie::RecyclingTrie<ValueT> reuse_trie_value;
	trie::RecyclingTrie<StaticValueT> reuse_static_value_trie;

	while(true) {
		float res = 0;
		reuse_trie.clear();
		reuse_trie_value.clear();
		reuse_static_value_trie.clear();
		switch(test) {
			case 0:
				res = test_std_set(accounts);
				break;
			case 1:
				res = test_std_unordered_set(accounts);
				break;
			case 2:
				res = test_std_map_emptyvalue(accounts);
				break;
			case 3:
				res = test_std_unordered_map_emptyvalue(accounts);
				break;
			case 4:
				res = test_smallnode_trie_emptyvalue(accounts);
				break;
			case 5:
				res = test_merkle_trie_emptyvalue(accounts);
				break;
			case 6:
				res = test_smallnode_trie_emptyvalue_reuse(accounts, reuse_trie);
				break;
			case 7:
				res = test_smallnode_trie_reuse(accounts, reuse_trie_value);
				break;
			case 8:
				res = test_std_map(accounts);
				break;
			case 9:
				res = test_std_unordered_map(accounts);
				break;
			case 10:
				res = test_smallnode_trie_reuse_offer(accounts, reuse_static_value_trie);
				break;
			case 11:
				res = test_merkle_trie(accounts);
				break;
			case 12:
				res = test_smallnode_trie_reuse_offer_hash(accounts, reuse_static_value_trie);
				break;
			case 13:
				res = test_smallnode_trie_reuse_txlog_hash(accounts, reuse_trie_value);
				break;
			default:
				throw std::runtime_error("invalid experiment number");

		}
		std::printf("%lf\n", res);
	}
}
