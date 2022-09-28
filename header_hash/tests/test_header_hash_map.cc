#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "mtt/utils/serialize_endian.h"
#include "utils/manage_data_dirs.h"
#include "utils/hash.h"

#include "header_hash/block_header_hash_map.h"

#include "xdr/types.h"
#include "xdr/block.h"

using xdr::operator==;

namespace speedex {

Block make_block(uint64_t value, uint64_t block_number) {
	Block buf;
	utils::write_unsigned_big_endian(buf.prevBlockHash, value);
	buf.blockNumber = block_number;
	return buf;
}

TEST_CASE("good insert", "[header]")
{
	test::speedex_dirs s;

	BlockHeaderHashMap map;

	map.insert(make_block(0x1234, 1), true);

	Hash hash1;
	map.hash(hash1);

	map.insert(make_block(0x2341, 2), true);
	
	Hash hash2;
	map.hash(hash2);

	REQUIRE(hash1 != hash2);
}

TEST_CASE("bad first insert", "[header]")
{
	test::speedex_dirs s;
	BlockHeaderHashMap map;
	Hash hash;

	REQUIRE_THROWS(map.insert(make_block(0, 2), true));
}

TEST_CASE("mixed insert", "[header]")
{
	test::speedex_dirs s;

	BlockHeaderHashMap map;

	map.insert(make_block(0x1234, 1), true);

	Hash hash1;
	map.hash(hash1);

	REQUIRE_THROWS(map.insert(make_block(0x2341, 1), true));
}

TEST_CASE("lmdb persist", "[header]")
{

	test::speedex_dirs s;
	
	BlockHeaderHashMap map;

	map.open_lmdb_env();
	map.create_lmdb();

	map.insert(make_block(0x1234, 1), true);

	map.insert(make_block(0x1235, 2), true);

	map.persist_lmdb(0);
	map.persist_lmdb(1);
	map.persist_lmdb(2);

	REQUIRE_THROWS(map.persist_lmdb(3));
}

TEST_CASE("lmdb rollback")
{
	test::speedex_dirs s;
	BlockHeaderHashMap map;

	map.open_lmdb_env();
	map.create_lmdb();

	map.insert(make_block(0x1234, 1), true);

	Hash hash1;
	map.hash(hash1);

	map.insert(make_block(0x1235, 2), true);

	Hash hash3;
	map.hash(hash3);

	map.persist_lmdb(1);
	map.rollback_to_committed_round(1);

	Hash hash2;
	map.hash(hash2);

	REQUIRE(hash1 == hash2);
	REQUIRE(hash1 !=  hash3);
}

TEST_CASE("lmdb reload", "[header]")
{
	test::speedex_dirs s;
	Hash first_hash;

	Hash round2_recall_hash;
	{
		BlockHeaderHashMap map;
		
		map.open_lmdb_env();
		map.create_lmdb();

		map.insert(make_block(0x1234, 1), true);

		Hash hash1;
		map.hash(hash1);

		Block block2 = make_block(0x1235, 2);

		round2_recall_hash = hash_xdr(block2);

		map.insert(block2, true);

		map.persist_lmdb(2);

		map.hash(first_hash);
	}
	
	BlockHeaderHashMap map;

	map.open_lmdb_env();
	map.open_lmdb();
	map.load_lmdb_contents_to_memory();

	Hash second_hash;
	map.hash(second_hash);

	REQUIRE(first_hash == second_hash);

	Block expected_block = make_block(0x1235, 2);
	Hash expected_hash = hash_xdr(expected_block);

	auto test_recall = map.get(2);

	REQUIRE((bool)test_recall);
	REQUIRE(test_recall -> hash == expected_hash);

	// ensure we don't overwrite persisted data post reload
	REQUIRE_THROWS(map.insert(expected_block, true));
}

} // speedex
