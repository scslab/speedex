#include <cxxtest/TestSuite.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "trie/recycling_impl/trie.h"
#include "trie/utils.h"

#include "utils/big_endian.h"
#include "utils/debug_macros.h"

#include "xdr/types.h"

#include <sodium.h>

using namespace speedex;

using xdr::operator==;

class RecyclingImplTestSuite : public CxxTest::TestSuite {
public:


	void test_empty_hash() {
		AccountTrie<EmptyValue> trie;
		Hash hash;

		trie.hash(hash);

		AccountTrie<XdrTypeWrapper<Hash>> trie2;

		Hash hash2;
		trie2.hash(hash2);

		TS_ASSERT_EQUALS(hash, hash2);
	}

	void test_empty_hash2() {
		AccountTrie<EmptyValue> trie;

		Hash h1;
		trie.hash(h1);

		SerialAccountTrie<EmptyValue> serial_trie = trie.open_serial_subsidiary();

		trie.merge_in(serial_trie);

		Hash h2;
		trie.hash(h2);

		TS_ASSERT_EQUALS(h1, h2);
	}
};
