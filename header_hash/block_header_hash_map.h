#pragma once
/*! \file block_header_hash_map.h

The block header-hash map is a merkle trie mapping block number to block hash.

Possible future optimization: Block numbers increment sequentially.
Once some subtrie fills up, it will never be modified again.  We don't need
to load that data into memory.  Would only be relevant if Speedex runs for
millions of blocks.
*/

#include "lmdb/lmdb_wrapper.h"
#include "lmdb/lmdb_loading.h"

#include <mtt/trie/merkle_trie.h>
#include <mtt/trie/metadata.h>
#include <mtt/trie/prefix.h>

#include "xdr/block.h"
#include "xdr/types.h"

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>

#include <xdrpp/marshal.h>

namespace speedex
{

// After commitment of block N, contains hashes of rounds 1 to N, *inclusive*

/*! LMDB instance for persisting block header hashes to disk
 */
struct BlockHeaderHashMapLMDB : public lmdb::LMDBInstance
{
    constexpr static auto DB_NAME = "header_hash_lmdb";

    BlockHeaderHashMapLMDB()
        : LMDBInstance()
    {}

    void open_env();

    void create_db() { LMDBInstance::create_db(DB_NAME); }

    void open_db() { LMDBInstance::open_db(DB_NAME); }
};

/*! Stores a merkle trie mapping block numbers to block root hashes.
 */
class BlockHeaderHashMap
{

    static std::vector<uint8_t> serialize(const BlockHeaderHashValue& v)
    {
        return xdr::xdr_to_opaque(v);
    }

    using ValueT = trie::XdrTypeWrapper<BlockHeaderHashValue, &serialize>;

    using prefix_t = trie::UInt64Prefix;

    using MetadataT = trie::CombinedMetadata<trie::SizeMixin>;

    using TrieT = trie::MerkleTrie<prefix_t, ValueT, MetadataT>;

    static_assert(TrieT::LOCKABLE, "need locks in internal block map");

    TrieT block_map;

    BlockHeaderHashMapLMDB lmdb_instance;

    uint64_t last_committed_block_number;

    // separate locks: one for managing access to lmdb (it's convenient
    // to lock access, ensuring get_persisted_round_number() results
    // are always consistent) and one managing access to local state.
    mutable std::mutex lmdb_mtx, last_committed_block_number_mtx;

public:
    constexpr static unsigned int KEY_LEN = sizeof(uint64_t);

    //! Construct empty map.
    BlockHeaderHashMap();

    /*! Insert hash of a newly produced block.
            In normal operation, map should include hashes for
            [0, last_committed_block_number) and block_number input is
            prev_block = last_committed_block_number
    */
    void insert(const Block& block, bool validation_success);

    //! Hash the merkle trie.
    void hash(Hash& hash);

    void open_lmdb_env() { lmdb_instance.open_env(); }
    void create_lmdb() { lmdb_instance.create_db(); }
    void open_lmdb() { lmdb_instance.open_db(); }

    //! Persist block hashes to LMDB, up to current block number.
    void persist_lmdb(uint64_t current_block_number);

    void rollback_to_committed_round(uint64_t committed_block_number);

    //! Get the block number reflected in disk state.
    uint64_t get_persisted_round_number() const
    {
        return lmdb_instance.get_persisted_round_number();
    }

    //! Read in trie contents from disk.
    void load_lmdb_contents_to_memory();

    std::optional<BlockHeaderHashValue> get(uint64_t round_number) const;
};

//! Mock around BlockHeaderHashMap that makes calls into no-ops when replaying
//! a block who's state changes are already reflected in lmdb.
class LoadLMDBHeaderMap : public LMDBLoadingWrapper<BlockHeaderHashMap&>
{

    using LMDBLoadingWrapper<BlockHeaderHashMap&>::generic_do;

public:
    LoadLMDBHeaderMap(uint64_t current_block_number,
                      BlockHeaderHashMap& main_db)
        : LMDBLoadingWrapper<BlockHeaderHashMap&>(current_block_number, main_db)
    {}

    //! Insert a block hash when replaying trusted blocks.
    void insert_for_loading(Block const& block, bool validation_success);
};

} // namespace speedex