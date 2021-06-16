#include <cxxtest/TestSuite.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "merkle_trie.h"
#include "merkle_work_unit.h"

#include "xdr/transaction.h"

#include "simple_debug.h"

#include <openssl/sha.h>

#include <iostream>

#include "price_utils.h"

using namespace edce;

class MerkleTrieTestSuite : public CxxTest::TestSuite {
public:
	void test_prefix_branch_bits() {
		uint32_t query = 0x12345678;

		MerkleTrie<4> :: prefix_t key_buf;

		PriceUtils::write_unsigned_big_endian(key_buf, query);
		//std::printf("%s\n", key_buf.to_string(key_buf.len()).c_str());

		TS_ASSERT_EQUALS(key_buf.get_branch_bits(PrefixLenBits{0}), 0x1);
		TS_ASSERT_EQUALS(key_buf.get_branch_bits(PrefixLenBits{4}), 0x2);
		TS_ASSERT_EQUALS(key_buf.get_branch_bits(PrefixLenBits{8}), 0x3);
		TS_ASSERT_EQUALS(key_buf.get_branch_bits(PrefixLenBits{12}), 0x4);
		TS_ASSERT_EQUALS(key_buf.get_branch_bits(PrefixLenBits{16}), 0x5);
		TS_ASSERT_EQUALS(key_buf.get_branch_bits(PrefixLenBits{20}), 0x6);
		TS_ASSERT_EQUALS(key_buf.get_branch_bits(PrefixLenBits{24}), 0x7);
		TS_ASSERT_EQUALS(key_buf.get_branch_bits(PrefixLenBits{28}), 0x8);
	}

	void test_prefix_match_len() {
		uint64_t query = 0xF000'0000;

		MerkleTrie<8> :: prefix_t key_buf;

		PriceUtils::write_unsigned_big_endian(key_buf, query);

		uint64_t query2 = 0xFF00'0000;
		MerkleTrie<8> :: prefix_t key_buf2;
		PriceUtils::write_unsigned_big_endian(key_buf2, query2);

		TS_ASSERT_EQUALS(key_buf.get_prefix_match_len(PrefixLenBits{64}, key_buf2, PrefixLenBits{64}), PrefixLenBits{36});
	}

	void test_truncate() {
		uint32_t query = 0x12345678;

		MerkleTrie<4> :: prefix_t key_buf;

		PriceUtils::write_unsigned_big_endian(key_buf, query);

		MerkleTrie<4> :: prefix_t key_buf2;


		uint32_t truncated = 0x12340000;

		PriceUtils::write_unsigned_big_endian(key_buf2, truncated);

		key_buf.truncate(PrefixLenBits{16});

		TS_ASSERT_EQUALS(key_buf, key_buf2);

		truncated = 0x12300000;
		PriceUtils::write_unsigned_big_endian(key_buf2, truncated);
		key_buf.truncate(PrefixLenBits{12});
		TS_ASSERT_EQUALS(key_buf, key_buf2);
	}


	void test_insert() {
		TEST_START();
		MerkleTrie<32> trie;
		MerkleTrie<32> :: prefix_t key_buf;

	//	unsigned char input_data[32];

		for (unsigned char i = 0; i < 10; i++) {
			std::array<unsigned char, 32> buf;
			SHA256(&i, 1, buf.data());
			//for (int i = 0; i < 32; i++) {
			//	std::cout<<std::hex<<(int)input_data[i]<<" ";
			//}
			//std::cout<<std::endl;
			key_buf = MerkleTrie<32> :: prefix_t(buf);
			trie.insert(key_buf);
		}

		TS_ASSERT_EQUALS(10, trie.uncached_size());
	}

	void test_short_key() {
		TEST_START();
		MerkleTrie<1> trie;
		MerkleTrie<1> :: prefix_t key_buf;
		for (unsigned char i = 0; i < 100; i += 10) {
			INFO("inserting %x", i);
			PriceUtils::write_unsigned_big_endian(key_buf, i);
		//	trie._log("pos: ");
			trie.insert(key_buf);
		}
		//trie._log("foo");
		TS_ASSERT_EQUALS(10, trie.uncached_size());
		INFO("finished first round of insertions");
		for (unsigned char i = 0; i < 100; i += 10) {
			INFO("inserting %x", i);
			PriceUtils::write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
		}
		TS_ASSERT_EQUALS(10, trie.uncached_size());
		//trie._log("test_short_key trie ");
	}

	void test_hash() {
		TEST_START();
		MerkleTrie<2> trie;
		MerkleTrie<2> :: prefix_t key_buf;

		Hash hash1, hash2;

//		unsigned char hash1[32];
//		unsigned char hash2[32];

		for (uint16_t i = 0; i < 1000; i+= 20) {
			PriceUtils::write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
		}
		TS_ASSERT_EQUALS(50, trie.uncached_size());

		trie.freeze_and_hash(hash1);
		
		for (uint16_t i = 0; i < 1000; i+= 20) {
			PriceUtils::write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
		}
		TS_ASSERT_EQUALS(50, trie.uncached_size());
		trie.freeze_and_hash(hash2);

		TS_ASSERT_EQUALS(0, memcmp(hash1.data(), hash2.data(), 32));

		uint16_t k = 125;
		PriceUtils::write_unsigned_big_endian(key_buf, k);
		trie.insert(key_buf);
		trie.freeze_and_hash(hash2);
		TS_ASSERT_DIFFERS(0, memcmp(hash1.data(), hash2.data(), 32));
	}

	void test_freeze_novalue() {
		TEST_START();
		MerkleTrie<2> trie;
		MerkleTrie<2> :: prefix_t key_buf;

		Hash hash1, hash2;

		for (uint16_t i = 0; i < 1000; i+= 20) {
			PriceUtils::write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
		}
		TS_ASSERT_EQUALS(50, trie.uncached_size());
		trie.freeze_and_hash(hash1);

		//auto frozen = FrozenMerkleTrie<2>(trie);

		trie.freeze_and_hash(hash2);
		TS_ASSERT_EQUALS(0, memcmp(hash1.data(), hash2.data(), 32));
	}

	/*void foo_test_freeze_destructive() { // we don't have destructive freeze anymore
		TEST_START();
		MerkleTrie<2, FrozenValue> trie;
		MerkleTrie<2, FrozenValue> :: prefix_t key_buf;

		Hash hash1, hash2;

		for (uint16_t i = 0; i < 1000; i+= 20) {
			PriceUtils::write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf, FrozenValue((unsigned char*) &i, 2));
		}
		TS_ASSERT_EQUALS(50, trie.uncached_size());
		trie.freeze_and_hash(hash1);

		auto frozen = trie.destructive_freeze();

		frozen.get_hash(hash2);

		TS_ASSERT_EQUALS(0, memcmp(hash1.data(), hash2.data(), 32));

	}*/

	void test_merge_novalue_simple() {
		TEST_START();
		MerkleTrie<2> trie;
		MerkleTrie<2> trie2;
		MerkleTrie<2> :: prefix_t key_buf;
		Hash hash1, hash2;

		for (uint16_t i = 0; i < 100; i+= 20) {
			INFO("inserting %d", i);
			PriceUtils::write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
			trie2.insert(key_buf);
		}
		TS_ASSERT_EQUALS(5, trie.uncached_size());
		TS_ASSERT_EQUALS(5, trie2.uncached_size());

		trie.freeze_and_hash(hash1);
		trie2.freeze_and_hash(hash2);
		TS_ASSERT_EQUALS(0, memcmp(hash1.data(), hash2.data(), 32));

		trie.merge_in(std::move(trie2));

		trie.freeze_and_hash(hash2);
		TS_ASSERT_EQUALS(0, memcmp(hash1.data(), hash2.data(), 32));
	}

	void test_merge_value_simple() {
		TEST_START();


		MerkleTrie<2, EmptyValue, CombinedMetadata<SizeMixin>> trie;
		MerkleTrie<2, EmptyValue, CombinedMetadata<SizeMixin>> trie2;
		MerkleTrie<2, EmptyValue, CombinedMetadata<SizeMixin>> :: prefix_t key_buf;

		Hash hash1, hash2;

		for (uint16_t i = 0; i < 100; i+= 20) {
			INFO("inserting %d", i);
			PriceUtils::write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
			trie2.insert(key_buf);
		}
		TS_ASSERT_EQUALS(5, trie.uncached_size());
		TS_ASSERT_EQUALS(5, trie2.uncached_size());

		trie.freeze_and_hash(hash1);
		trie2.freeze_and_hash(hash2);
		TS_ASSERT_EQUALS(0, memcmp(hash1.data(), hash2.data(), 32));

		trie.merge_in(std::move(trie2));

		trie.freeze_and_hash(hash2);
		TS_ASSERT_EQUALS(0, memcmp(hash1.data(), hash2.data(), 32));

		TS_ASSERT(trie.partial_metadata_integrity_check());
		TS_ASSERT(trie.metadata_integrity_check());
	}



	void check_equality(MerkleTrie<2>& t1, MerkleTrie<2>& t2) {
		Hash hash1, hash2;

		t1.freeze_and_hash(hash1);
		t2.freeze_and_hash(hash2);
		TS_ASSERT_EQUALS(0, memcmp(hash1.data(), hash2.data(), 32));
		TS_ASSERT_EQUALS(t1.uncached_size(), t2.uncached_size());
	}

	void test_merge_some_shared_keys() {
		TEST_START();
		MerkleTrie<2> trie;
		MerkleTrie<2> mergein;
		MerkleTrie<2> expect;
		MerkleTrie<2> :: prefix_t key;



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
		MerkleTrie<2> trie;
		MerkleTrie<2> :: prefix_t key_buf;
		for (uint16_t i = 0; i < 1000; i+=20) {
			PriceUtils::write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf);
		}

		TS_ASSERT_EQUALS(50, trie.uncached_size());

		for (uint16_t i = 0; i < 1000; i += 40) {
			PriceUtils::write_unsigned_big_endian(key_buf, i);
			TS_ASSERT(trie.perform_deletion(key_buf));
		}

		TS_ASSERT_EQUALS(25, trie.uncached_size());
	}

	using OfferWrapper = XdrTypeWrapper<Offer>;

	void test_split() {
		TEST_START();
		using TrieT = MerkleTrie<2, OfferWrapper, MerkleWorkUnit::TrieMetadataT>;

		TrieT trie;
		TrieT::prefix_t key_buf;

		Offer offer;
		offer.amount = 10;
		offer.minPrice = 1;

		for (uint16_t i = 0; i < 1000; i+=20) {
			PriceUtils::write_unsigned_big_endian(key_buf, i);
			trie.insert(key_buf, OfferWrapper(offer));
		}
		TS_ASSERT_EQUALS(50, trie.size());

		auto split = trie.endow_split(5);//trie.metadata_split<EndowmentPredicate>(5);

		TS_ASSERT_EQUALS(split.size(), 0);

		auto split2 = trie.endow_split(10);//metadata_split<EndowmentPredicate>(10);
		
		TS_ASSERT_EQUALS(split2.size(), 1);
		TS_ASSERT_EQUALS(trie.size(), 49);


		auto split3 = trie.endow_split(15);//metadata_split<EndowmentPredicate>(15);
		TS_ASSERT_EQUALS(split3.size(), 1);
		TS_ASSERT_EQUALS(trie.size(), 48);

		auto split4 = trie.endow_split(252);//metadata_split<EndowmentPredicate>(252);

		TS_ASSERT_EQUALS(split4.size(), 25);
		TS_ASSERT_EQUALS(trie.size(), 23);

		TS_ASSERT(trie.partial_metadata_integrity_check());
	}

	void test_endow_below_threshold() {
		TEST_START();
		using ValueT = XdrTypeWrapper<Offer>;

		using TrieT = MerkleTrie<2, ValueT, MerkleWorkUnit::TrieMetadataT>;

		TrieT trie;
		TrieT::prefix_t buf;

		ValueT offer;
		offer.amount = 10;
		offer.minPrice = 1;

		for (uint16_t i = 0; i < 1000; i+=20) {
			PriceUtils::write_unsigned_big_endian(buf, i);
			trie.insert(buf, XdrTypeWrapper<Offer>(offer));
		}
		TS_ASSERT_EQUALS(50, trie.size());

		uint16_t threshold = 35;
		PriceUtils::write_unsigned_big_endian(buf, threshold);

		TS_ASSERT_EQUALS(trie.endow_lt_key(buf), 20);

		threshold = 20;
		PriceUtils::write_unsigned_big_endian(buf, threshold);

		TS_ASSERT_EQUALS(trie.endow_lt_key(buf), 10);

		threshold = 21;
		PriceUtils::write_unsigned_big_endian(buf, threshold);
		TS_ASSERT_EQUALS(trie.endow_lt_key(buf), 20);

		threshold = 500;
		PriceUtils::write_unsigned_big_endian(buf, threshold);

		TS_ASSERT_EQUALS(trie.endow_lt_key(buf), 250);


		threshold = 2000;
		PriceUtils::write_unsigned_big_endian(buf, threshold);
		TS_ASSERT_EQUALS(trie.endow_lt_key(buf), 500);


	}

};
