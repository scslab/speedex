#include "block_header_hash_map.h"

#include "config.h"

#include <utils/serialize_endian.h>

#include "utils/hash.h"

#include "utils/debug_macros.h"
#include "utils/debug_utils.h"

using lmdb::dbval;

namespace speedex
{

using xdr::operator==;

using key_byte_array_t = std::array<uint8_t, BlockHeaderHashMap::KEY_LEN>;

void
BlockHeaderHashMapLMDB::open_env()
{
    LMDBInstance::open_env(
        std::string(ROOT_DB_DIRECTORY) + std::string(HEADER_HASH_DB),
        DEFAULT_LMDB_FLAGS | MDB_NOLOCK); // need NOLOCK for commit/reload tests
}

BlockHeaderHashMap::BlockHeaderHashMap()
    : block_map()
    , lmdb_instance()
    , last_committed_block_number(0)
    , mtx()
{}

void
BlockHeaderHashMap::insert(const Block& block, bool res)
{

    std::lock_guard lock(mtx);

    uint64_t block_number = block.blockNumber;
    if (block_number == 0)
    {
        throw std::runtime_error("should never insert genesis hash!");
    }

    // block header hash map requires strict sequentiality, unlike memdb thunks
    // & orderbook thunks
    if (block_number != last_committed_block_number + 1)
    {
        throw std::runtime_error("inserting wrong block number");
    }

    prefix_t key_buf;

    utils::write_unsigned_big_endian(key_buf, block_number);

    BlockHeaderHashValue value;
    value.hash = hash_xdr(block);
    value.validation_success = res ? 1 : 0;

    block_map.insert(key_buf, ValueT(value));

    last_committed_block_number = block_number;
}

void
BlockHeaderHashMap::persist_lmdb(uint64_t current_block_number)
{

    std::lock_guard lock(mtx);

    BLOCK_INFO("persisting header hash map at round %lu", current_block_number);

    if (!lmdb_instance)
    {
        return;
    }
    uint64_t persisted_round_number
        = lmdb_instance.get_persisted_round_number();

    auto wtx = lmdb_instance.wbegin();

    // changed: we commit current_block_number because we've already inserted
    // it.
    for (uint64_t i = persisted_round_number; i <= current_block_number; i++)
    {
        if (i == 0)
            continue;
        TrieT::prefix_t round_buf;

        utils::write_unsigned_big_endian(round_buf, i);

        auto round_bytes
            = round_buf.template get_bytes_array<key_byte_array_t>();
        dbval key{ round_bytes };

        // querying for round i
        auto hash_opt = block_map.get_value(round_buf);
        if (!hash_opt)
        {
            throw std::runtime_error("did not find hash in hash_map!");
        }

        BlockHeaderHashValue unwrapped_value = *hash_opt;

        auto cur_bytes = xdr::xdr_to_opaque(unwrapped_value);

        dbval hash_val{ cur_bytes };
        wtx.put(lmdb_instance.get_data_dbi(), &key, &hash_val);
    }

    lmdb_instance.commit_wtxn(wtx, current_block_number);
}

// LMDB committed to round X contains entries 1 through X.
// To sync back with LMDB, we need to remove all entries X+1 and higher.

void
BlockHeaderHashMap::rollback_to_committed_round(uint64_t committed_block_number)
{
    std::lock_guard lock(mtx);

    if (committed_block_number < lmdb_instance.get_persisted_round_number())
    {
        throw std::runtime_error("can't rollback beyond lmdb persist");
    }
    for (uint64_t i = committed_block_number + 1;
         i <= last_committed_block_number;
         i++)
    {
        if (i == 0)
            continue;
        TrieT::prefix_t round_buf;
        utils::write_unsigned_big_endian(round_buf, i);

        if (!block_map.perform_deletion(round_buf))
        {
            throw std::runtime_error(
                "error when deleting from header hash map");
        }
    }
    last_committed_block_number
        = committed_block_number; //(committed_block_number == 0) ? 0 :
                                  // committed_block_number - 1;
}

void
BlockHeaderHashMap::load_lmdb_contents_to_memory()
{

    std::lock_guard lock(mtx);

    auto rtx = lmdb_instance.rbegin();

    auto cursor = rtx.cursor_open(lmdb_instance.get_data_dbi());

    for (auto kv : cursor)
    {
        auto bytes = kv.first.bytes();

        TrieT::prefix_t prefix;
        prefix.from_bytes_array(bytes);

        uint64_t round_number;

        utils::read_unsigned_big_endian(prefix, round_number);

        if (round_number > lmdb_instance.get_persisted_round_number())
        {

            std::printf("round number: %" PRIu64
                        " persisted_round_number: %" PRIu64 "\n",
                        round_number,
                        lmdb_instance.get_persisted_round_number());
            std::fflush(stdout);
            throw std::runtime_error(
                "lmdb contains round idx beyond committed max");
        }

        prefix_t round_buf;
        utils::write_unsigned_big_endian(round_buf, round_number);

        BlockHeaderHashValue block;
        // if (kv.second.mv_size != 32) {
        //	throw std::runtime_error("invalid value size");
        // }

        auto db_value_bytes = kv.second.bytes();
        xdr::xdr_from_opaque(db_value_bytes, block);

        // memcpy(value.data(), kv.second.mv_data, 32);
        block_map.insert(round_buf, ValueT(block));
    }
    last_committed_block_number = lmdb_instance.get_persisted_round_number();
    rtx.commit();
}

std::optional<BlockHeaderHashValue>
BlockHeaderHashMap::get(uint64_t round_number) const
{

    std::lock_guard lock(mtx);

    if (round_number > get_persisted_round_number())
    {
        return std::nullopt;
    }

    prefix_t round_buf;
    utils::write_unsigned_big_endian(round_buf, round_number);
    auto* out_opt = block_map.get_value(round_buf);

    if (!out_opt)
    {
        throw std::runtime_error("failed to load hash that lmdb should have");
    }

    return { *out_opt };
}

void
LoadLMDBHeaderMap::insert_for_loading(Block const& block,
                                      bool validation_success)
{
    return generic_do<&BlockHeaderHashMap::insert>(block, validation_success);
}

} // namespace speedex
