#include "hotstuff/lmdb.h"

#include "utils/big_endian.h"

namespace hotstuff {

using speedex::Hash;
using speedex::dbval;

HotstuffLMDB::cursor::iterator::kv_t 
HotstuffLMDB::cursor::iterator::operator*() {
	auto const& [k, v] = *it;

	uint64_t key_parsed;

	auto k_bytes = k.bytes();
	speedex::read_unsigned_big_endian(k_bytes, key_parsed);

	Hash hash;
	std::vector<uint8_t> hash_bytes;
	auto value_bytes = v.bytes();
	hash_bytes.insert(hash_bytes.end(), value_bytes.begin(), value_bytes.begin() + hash.size());
	xdr::xdr_from_opaque(hash_bytes, hash);

	return {key_parsed, hash};
}

void 
HotstuffLMDB::add_decided_block_(block_ptr_t blk, std::vector<uint8_t> const& serialized_vm_blk_id) {

	uint64_t height = blk->get_height();
	Hash const& hash = blk->get_hash();

	auto wtx = wbegin();

	std::array<uint8_t, 8> key_bytes;
	speedex::write_unsigned_big_endian(key_bytes, height);

	std::vector<uint8_t> value_bytes;
	value_bytes.insert(value_bytes.end(), hash.begin(), hash.end());
	value_bytes.insert(value_bytes.end(), serialized_vm_blk_id.begin(), serialized_vm_blk_id.end());

	dbval value_val{value_bytes};
	dbval key_val{key_bytes};

	wtx.put(get_data_dbi(), key_val, value_val);
	commit_wtxn(wtx, blk -> get_height());
}

std::optional<std::pair<Hash, std::vector<uint8_t>>>
HotstuffLMDB::get_decided_hash_id_pair_(uint64_t height) const {
	std::array<unsigned char, 8> bytes;
	speedex::write_unsigned_big_endian(bytes, height);
	dbval key_val{bytes};
	

	auto rtxn = rbegin();
	auto value = rtxn.get(get_data_dbi(), key_val);

	if (!value) {
		return std::nullopt;
	}

	Hash hash;

	std::vector<uint8_t> hash_bytes;

	auto value_bytes = value -> bytes();

	hash_bytes.insert(hash_bytes.end(), value_bytes.begin(), value_bytes.begin() + hash.size());
	xdr::xdr_from_opaque(hash_bytes, hash);
	return {{hash, value_bytes}};
}

} /* hotstuff */
