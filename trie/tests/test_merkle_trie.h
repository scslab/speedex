#include <cxxtest/TestSuite.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "trie/merkle_trie.h"
//#include "merkle_work_unit.h"

#include "orderbook/metadata.h"

#include "xdr/transaction.h"

#include "utils/big_endian.h"
#include "utils/debug_macros.h"

#include <sodium.h>

#include <iostream>


using namespace speedex;

using xdr::operator==;

class MerkleTrieTestSuite : public CxxTest::TestSuite {
public:

	void test_insert() {
		TEST_START();

		using mt = MerkleTrie<ByteArrayPrefix<32>>;

		mt trie;
		mt :: prefix_t key_buf;


		for (unsigned char i = 0; i < 10; i++) {
			std::array<unsigned char, 32> buf;
			crypto_generichash(buf.data(), buf.size(), &i, 1, NULL, 0);
			//SHA256(&i, 1, buf.data());

			key_buf = mt :: prefix_t(buf);
			trie.insert(key_buf);
		}

		TS_ASSERT_EQUALS(10u, trie.uncached_size());
	}

	void test_short_key() {
		TEST_START();
		using mt = MerkleTrie<ByteArrayPrefix<1>>;
		mt trie;
		mt :: prefix_t key_buf;
		for (unsigned char i = 0; i < 100; i += 10) {
			write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
		}
		TS_ASSERT_EQUALS(10u, trie.uncached_size());
		for (unsigned char i = 0; i < 100; i += 10) {
			write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
		}
		TS_ASSERT_EQUALS(10u, trie.uncached_size());
	}

	void test_hash() {
		TEST_START();

		using mt = MerkleTrie<ByteArrayPrefix<2>>;
		mt trie;
		mt :: prefix_t key_buf;

		Hash hash1, hash2;

		for (uint16_t i = 0; i < 1000; i+= 20) {
			write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
		}
		TS_ASSERT_EQUALS(50u, trie.uncached_size());

		trie.hash(hash1);
		
		for (uint16_t i = 0; i < 1000; i+= 20) {
			write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
		}
		TS_ASSERT_EQUALS(50u, trie.uncached_size());
		trie.hash(hash2);

		TS_ASSERT_EQUALS(hash1, hash2);

		uint16_t k = 125;
		write_unsigned_big_endian(key_buf, k);
		trie.insert(key_buf);
		trie.hash(hash2);
		TS_ASSERT_DIFFERS(hash1, hash2);
	}

	void test_merge_novalue_simple() {
		TEST_START();

		using mt = MerkleTrie<ByteArrayPrefix<2>>;
		mt trie;
		mt trie2;
		mt :: prefix_t key_buf;
		Hash hash1, hash2;

		for (uint16_t i = 0; i < 100; i+= 20) {
			INFO("inserting %d", i);
			write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
			trie2.insert(key_buf);
		}
		TS_ASSERT_EQUALS(5u, trie.uncached_size());
		TS_ASSERT_EQUALS(5u, trie2.uncached_size());

		trie.hash(hash1);
		trie2.hash(hash2);
		TS_ASSERT_EQUALS(hash1, hash2);
		//TS_ASSERT_EQUALS(0, memcmp(hash1.data(), hash2.data(), 32));

		trie.merge_in(std::move(trie2));

		trie.hash(hash2);
		TS_ASSERT_EQUALS(hash1, hash2);
		//TS_ASSERT_EQUALS(0, memcmp(hash1.data(), hash2.data(), 32));
	}

	void test_merge_value_simple() {
		TEST_START();

		using mt = MerkleTrie<ByteArrayPrefix<2>, EmptyValue, CombinedMetadata<SizeMixin>>;

		mt trie;
		mt trie2;
		mt :: prefix_t key_buf;

		Hash hash1, hash2;

		for (uint16_t i = 0; i < 100; i+= 20) {
			INFO("inserting %d", i);
			write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
			trie2.insert(key_buf);
		}
		TS_ASSERT_EQUALS(5u, trie.uncached_size());
		TS_ASSERT_EQUALS(5u, trie2.uncached_size());

		trie.hash(hash1);
		trie2.hash(hash2);
		TS_ASSERT_EQUALS(hash1, hash2);
		//TS_ASSERT_EQUALS(0, memcmp(hash1.data(), hash2.data(), 32));

		trie.merge_in(std::move(trie2));

		trie.hash(hash2);
		TS_ASSERT_EQUALS(hash1, hash2);
		//TS_ASSERT_EQUALS(0, memcmp(hash1.data(), hash2.data(), 32));

		TS_ASSERT(trie.metadata_integrity_check());
	}



	void check_equality(MerkleTrie<ByteArrayPrefix<2>>& t1, MerkleTrie<ByteArrayPrefix<2>>& t2) {
		Hash hash1, hash2;

		t1.hash(hash1);
		t2.hash(hash2);
		TS_ASSERT_EQUALS(hash1, hash2);
		//TS_ASSERT_EQUALS(0, memcmp(hash1.data(), hash2.data(), 32));
		TS_ASSERT_EQUALS(t1.uncached_size(), t2.uncached_size());
	}

	void test_merge_some_shared_keys() {
		TEST_START();

		using mt = MerkleTrie<ByteArrayPrefix<2>>;
		mt trie;
		mt mergein;
		mt expect;
		mt :: prefix_t key;



		key[0] = 0xFF;
		key[1] = 0;
		trie.insert(key);
		mergein.insert(key);
		expect.insert(key);		

		//Full match (case 0)
		trie.merge_in(std::move(mergein));
		check_equality(trie, expect);

		// a branch (case 4)
		key[0] = 0xF0;
		mergein.clear();
		mergein.insert(key);
		expect.insert(key);

		trie.merge_in(std::move(mergein));

		check_equality(trie, expect);

		//trie._log(std::string("TRIE:  "));
		//expect._log(std::string("EXPC:  "));
		// case 2
		key[0] = 0xF1;
		mergein.clear();
		mergein.insert(key);
		expect.insert(key);

		trie.merge_in(std::move(mergein));
		check_equality(trie, expect);

		//trie._log(std::string("TRIE:  "));
		//expect._log(std::string("EXPC:  "));
		// case 3
		key[0] = 0xA0;
		mergein.clear();
		mergein.insert(key);
		expect.insert(key);
		key[0] = 0xA1;
		mergein.insert(key);
		expect.insert(key);
		key[0] = 0xA2;
		trie.insert(key);
		expect.insert(key);

		trie.merge_in(std::move(mergein));
		check_equality(trie, expect);

		//trie._log(std::string("TRIE:  "));
		//expect._log(std::string("EXPC:  "));
		//case 1
		key[0] = 0xA1;
		mergein.clear();
		mergein.insert(key);
		expect.insert(key);

		key[0] = 0xA3;
		mergein.insert(key);
		expect.insert(key);

		INFO("Starting last merge");
		trie.merge_in(std::move(mergein));
		INFO("starting eq check");
		check_equality(trie, expect);
	}

	void test_perform_delete() {
		TEST_START();
		using mt = MerkleTrie<ByteArrayPrefix<2>>;
		mt trie;
		mt :: prefix_t key_buf;
		for (uint16_t i = 0; i < 1000; i+=20) {
			write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
		}

		TS_ASSERT_EQUALS(50u, trie.uncached_size());

		for (uint16_t i = 0; i < 1000; i += 40) {
			write_unsigned_big_endian(key_buf, i);
			TS_ASSERT(trie.perform_deletion(key_buf));
		}

		TS_ASSERT_EQUALS(25u, trie.uncached_size());
	}

	using OfferWrapper = XdrTypeWrapper<Offer>;

	void test_split() {
		TEST_START();
		using TrieT = MerkleTrie<ByteArrayPrefix<2>, OfferWrapper, CombinedMetadata<OrderbookMetadata>>;

		TrieT trie;
		TrieT::prefix_t key_buf;

		Offer offer;
		offer.amount = 10;
		offer.minPrice = 1;

		for (uint16_t i = 0; i < 1000; i+=20) {
			write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf, OfferWrapper(offer));
		}
		TS_ASSERT_EQUALS(50u, trie.size());

		auto split = trie.endow_split(5);//trie.metadata_split<EndowmentPredicate>(5);

		TS_ASSERT_EQUALS(split.size(), 0u);

		auto split2 = trie.endow_split(10);//metadata_split<EndowmentPredicate>(10);
		
		TS_ASSERT_EQUALS(split2.size(), 1u);
		TS_ASSERT_EQUALS(trie.size(), 49u);


		auto split3 = trie.endow_split(15);//metadata_split<EndowmentPredicate>(15);
		TS_ASSERT_EQUALS(split3.size(), 1u);
		TS_ASSERT_EQUALS(trie.size(), 48u);

		auto split4 = trie.endow_split(252);//metadata_split<EndowmentPredicate>(252);

		TS_ASSERT_EQUALS(split4.size(), 25u);
		TS_ASSERT_EQUALS(trie.size(), 23u);

		TS_ASSERT(trie.metadata_integrity_check());
	}

	void test_endow_below_threshold() {
		TEST_START();
		using ValueT = XdrTypeWrapper<Offer>;

		using TrieT = MerkleTrie<ByteArrayPrefix<2>, ValueT, CombinedMetadata<OrderbookMetadata>>;

		TrieT trie;
		TrieT::prefix_t buf;

		ValueT offer;
		offer.amount = 10;
		offer.minPrice = 1;

		for (uint16_t i = 0; i < 1000; i+=20) {
			write_unsigned_big_endian(buf, i);
			trie.insert(buf, XdrTypeWrapper<Offer>(offer));
		}
		TS_ASSERT_EQUALS(50u, trie.size());

		uint16_t threshold = 35;
		write_unsigned_big_endian(buf, threshold);

		TS_ASSERT_EQUALS(trie.endow_lt_key(buf), 20);

		threshold = 20;
		write_unsigned_big_endian(buf, threshold);

		TS_ASSERT_EQUALS(trie.endow_lt_key(buf), 10);

		threshold = 21;
		write_unsigned_big_endian(buf, threshold);
		TS_ASSERT_EQUALS(trie.endow_lt_key(buf), 20);

		threshold = 500;
		write_unsigned_big_endian(buf, threshold);

		TS_ASSERT_EQUALS(trie.endow_lt_key(buf), 250);


		threshold = 2000;
		write_unsigned_big_endian(buf, threshold);
		TS_ASSERT_EQUALS(trie.endow_lt_key(buf), 500);
	}

	void test_empty_hash() {

		using ValueT = XdrTypeWrapper<Offer>;
		using TrieT = MerkleTrie<ByteArrayPrefix<2>, ValueT, CombinedMetadata<OrderbookMetadata>>;


		TrieT trie;
		Hash hash;

		trie.hash(hash);

		MerkleTrie<ByteArrayPrefix<2>, EmptyValue, CombinedMetadata<OrderbookMetadata>> trie2;

		Hash hash2;
		trie2.hash(hash2);

		TS_ASSERT_EQUALS(hash, hash2);
	}

};
