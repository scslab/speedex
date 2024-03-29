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

#include "orderbook/lmdb.h"

#include "utils/price.h"
#include "utils/debug_macros.h"

#include "lmdb/lmdb_loading.h"

#include "speedex/speedex_static_configs.h"

#include <cinttypes>
#include <set>

namespace speedex
{
using lmdb::dbval;

using get_bytes_array_t = std::array<uint8_t, ORDERBOOK_KEY_LEN>;

// Commits everything up to and including current_block_number
std::unique_ptr<ThunkGarbage<typename OrderbookLMDB::trie_t>> __attribute__((
    warn_unused_result))
OrderbookLMDB::write_thunks(const uint64_t current_block_number,
                            lmdb::dbenv::wtxn& wtx,
                            bool debug)
{

    // dbenv::wtxn wtx = wbegin();

    std::vector<thunk_t> relevant_thunks;
    {
        std::lock_guard lock(*mtx);
        for (size_t i = 0; i < thunks.size();)
        {
            auto& thunk = thunks.at(i);
            if (thunk.current_block_number <= current_block_number)
            {
                relevant_thunks.emplace_back(std::move(thunk));
                thunks.at(i).reset_trie();
                thunks.erase(thunks.begin() + i);
            } else
            {
                i++;
            }
        }
    }

    if constexpr (!DISABLE_LMDB)
    {
        // In one round, everything below partial_exec_key is deleted (cleared).
        // So first, we find the maximum partial_exec_key, and delete everything
        // BELOW that.

        // get maximum key, remove offers
        //  iterate from top to bot, adding only successful offers and rolling key
        //  downwards

        prefix_t key_buf;

        bool key_set = false;

        // not valid if thunk gaps are allowed
        //	if (relevant_thunks.size() == 0 && get_persisted_round_number() !=
        //current_block_number) { 		throw std::runtime_error("can't persist without
        //thunks");
        //	}

        if (relevant_thunks.size() == 0)
        {
            return nullptr;
        }

        // changed to inequality when thunk gaps were added
        if (relevant_thunks[0].current_block_number
            < get_persisted_round_number() + 1)
        {
            throw std::runtime_error("too small current_block_number");
        }

        // compute maximum key

        bool print = false;

        if (print)
            std::printf("phase 1\n");

        std::set<prefix_t> keys_that_will_be_later_deleted;

        for (uint32_t i = 0; i < relevant_thunks.size(); i++)
        {
            auto& thunk = relevant_thunks[i];
            if (thunk.current_block_number > current_block_number)
            {
                throw std::runtime_error("impossible");
                // inflight thunk that's not done yet, or for some reason we're not
                // committing that far out yet.
                continue;
            }
            if (print)
                std::printf("phase 1(max key) thunk i=%" PRIu32 " %" PRIu64 "\n",
                            i,
                            thunk.current_block_number);

            // remove deleted keys
            for (auto& delete_kv : thunk.deleted_keys.deleted_keys)
            {
                auto& delete_key = delete_kv.first;
                auto bytes = delete_key.template get_bytes_array<get_bytes_array_t>();
                dbval key = dbval{ bytes };
                if (!wtx.del(get_data_dbi(), key))
                {
                    keys_that_will_be_later_deleted.insert(delete_key);
                }
            }

            if (thunk.get_exists_partial_exec())
            {
                key_set = true;
                if (key_buf < thunk.partial_exec_key)
                {
                    key_buf = thunk.partial_exec_key;
                }
                INTEGRITY_CHECK(
                    "thunk threshold key: %s",
                    debug::array_to_str(thunk.partial_exec_key,
                                        MerkleWorkUnit::WORKUNIT_KEY_LEN)
                        .c_str());
            }
        }

        INTEGRITY_CHECK(
            "final max key: %s",
            debug::array_to_str(key_buf, MerkleWorkUnit::WORKUNIT_KEY_LEN).c_str());

        if (print)
            std::printf("phase 2\n");

        auto cursor = wtx.cursor_open(get_data_dbi());

        // auto begin_cursor = wtx.cursor_open(dbi).begin();

        auto key_buf_bytes = key_buf.template get_bytes_array<get_bytes_array_t>();

        dbval key = dbval{ key_buf_bytes };

        // MerkleTrieT::prefix_t key_backup = key_buf;
        // unsigned char key_backup[MerkleWorkUnit::WORKUNIT_KEY_LEN];
        // memcpy(key_backup, key_buf, MerkleWorkUnit::WORKUNIT_KEY_LEN);

        cursor.get(MDB_SET_RANGE, key);

        // get returns the least key geq  key_buf.
        // So after one --, we get the greatest key < key_buf.
        //  find greatest key leq key_buf;

        int num_deleted = 0;

        if ((!key_set) || (!cursor))
        {
            INTEGRITY_CHECK("setting cursor to last, key_set = %d", key_set);
            cursor.get(MDB_LAST);
            // If the get operation fails, then there isn't a least key greater than
            // key_buf. if key was not set by any thunk, then all thunks left db in
            // fully cleared state.
            //  so we have to delete everything on disk.

            // So we must delete the entire database.

        } else
        {
            --cursor;
        }

        while (cursor)
        {
            cursor.del();
            --cursor;
            num_deleted++;
        }

        // while (cursor != begin_cursor) {
        //	--cursor;
        //	cursor.del();

        //	num_deleted++;
        //}
        INTEGRITY_CHECK_F(if (num_deleted > 0) {
            INTEGRITY_CHECK("num deleted is %u", num_deleted);
        });

        key_buf.clear();

        if (print)
            std::printf("phase 3\n");

        // signed int is important for this for loop
        for (int32_t i = relevant_thunks.size() - 1; i >= 0; i--)
        {

            if (relevant_thunks.at(i).current_block_number > current_block_number)
            {
                // again ignore inflight thunks
                throw std::runtime_error("impossible");
                continue;
            }

            if (print)
                std::printf("phase 3 i %" PRId32 " %" PRIu64 "\n",
                            i,
                            relevant_thunks[i].current_block_number);

            if (key_buf < relevant_thunks[i].partial_exec_key)
            {
                key_buf = relevant_thunks[i].partial_exec_key;
            }

            Price min_exec_price = price::read_price_big_endian(key_buf);

            prefix_t offer_key_buf;

            if (print)
                std::printf("thunks[i].uncommitted_offers_vec.size() %" PRIu32
                            " , current_block_number %" PRIu64 "\n",
                            static_cast<uint32_t>(
                                relevant_thunks[i].uncommitted_offers_vec.size()),
                            relevant_thunks[i].current_block_number);

            for (auto idx = relevant_thunks[i].uncommitted_offers_vec.size();
                 idx > 0;
                 idx--)
            { // can't test an unsigned for >=0 meaningfully

                auto& cur_offer
                    = relevant_thunks[i]
                          .uncommitted_offers_vec[idx - 1]; // hence idx -1
                if (cur_offer.amount == 0)
                {
                    std::printf("cur_offer.owner %" PRIu64 " id %" PRIu64
                                " cur_offer.amount %" PRId64
                                " cur_offer.minPrice %" PRIu64 "\n",
                                cur_offer.owner,
                                cur_offer.offerId,
                                cur_offer.amount,
                                cur_offer.minPrice);
                    throw std::runtime_error("tried to persist an amount 0 offer!");
                }

                if (cur_offer.minPrice >= min_exec_price)
                {

                    generate_orderbook_trie_key(cur_offer, offer_key_buf);
                    bool db_put = false;
                    if (cur_offer.minPrice > min_exec_price)
                    {
                        db_put = true;
                    } else
                    {

                        if (offer_key_buf >= key_buf)
                        {
                            db_put = true;
                        }
                    }

                    if (keys_that_will_be_later_deleted.find(offer_key_buf)
                        != keys_that_will_be_later_deleted.end())
                    {
                        db_put = false;
                    }

                    if (db_put)
                    {

                        auto offer_key_buf_bytes = offer_key_buf.template get_bytes_array<get_bytes_array_t>();
                        dbval db_key = dbval{ offer_key_buf_bytes };

                        auto value_buf = xdr::xdr_to_opaque(cur_offer);
                        dbval value = dbval{ value_buf.data(), value_buf.size() };
                        try
                        {
                            wtx.put(get_data_dbi(), &db_key, &value);
                        }
                        catch (...)
                        {
                            std::printf("failed to insert offer to offer lmdb\n");
                            throw;
                        }
                    } else
                    {
                        break;
                    }
                } else
                {
                    break;
                }
            }
            if (print)
                std::printf("done phase 3 loop\n");
        }

        // insert the partial exec offers

        if (print)
            std::printf("phase 4\n");

        for (uint32_t i = 0; i < relevant_thunks.size(); i++)
        {

            if (relevant_thunks.at(i).current_block_number > current_block_number)
            {
                // again ignore inflight thunks
                throw std::runtime_error("impossible");
                continue;
            }

            if (print)
                std::printf("phase 4 i %" PRIu32 " %" PRIu64 "\n",
                            i,
                            relevant_thunks[i].current_block_number);

            // thunks[i].uncommitted_offers.apply_geq_key(func, key_buf);

            INTEGRITY_CHECK("thunks[i].uncommitted_offers.size() = %d",
                            relevant_thunks[i].uncommitted_offers.size());

            INTEGRITY_CHECK("num values put = %d", func.num_values_put);

            if (!relevant_thunks[i].get_exists_partial_exec())
            {
                INTEGRITY_CHECK("no partial exec, continuing to next thunk");
                continue;
            }

            auto partial_exec_key_bytes
                = relevant_thunks[i].partial_exec_key.template get_bytes_array<get_bytes_array_t>();
            dbval partial_exec_key{
                partial_exec_key_bytes
            }; //(relevant_thunks[i].partial_exec_key.data(),
               //MerkleWorkUnit::WORKUNIT_KEY_LEN);

            auto get_res = wtx.get(get_data_dbi(), partial_exec_key);

            if (!get_res)
            {
                INTEGRITY_CHECK(
                    "didn't find partial exec key because of preemptive clearing");
                continue;
                // throw std::runtime_error("did not find offer that should be in
                // lmdb");
            }

            // offer in memory
            Offer partial_exec_offer; // = thunks[i].preexecute_partial_exec_offer;

            // if (get_res) {
            // std::printf("partial exec of preexisting offer\n");
            // use offer on disk instead, if it exists.
            // This lets us process partial executions in backwards order.
            dbval_to_xdr(*get_res, partial_exec_offer);
            //}

            // Offer partial_exec_offer = thunks[i].preexecute_partial_exec_offer;

            if (relevant_thunks[i].partial_exec_amount < 0)
            {
                // allowed to be 0 if no partial exec
                std::printf("thunks[i].partial_exec_amount = %" PRId64 "\n",
                            relevant_thunks[i].partial_exec_amount);
                throw std::runtime_error("invalid thunks[i].partial_exec_amount");
            }

            if ((uint64_t)relevant_thunks[i].partial_exec_amount
                > partial_exec_offer.amount)
            {
                std::printf("relevant thunks[i].partial_exec_amount = %" PRId64
                            " partial_exec_offer.amount = %" PRId64 "\n",
                            relevant_thunks[i].partial_exec_amount,
                            partial_exec_offer.amount);
                throw std::runtime_error("can't have partial exec offer.amount < "
                                         "thunk[i].partial_exec_amount");
            }

            partial_exec_offer.amount -= relevant_thunks[i].partial_exec_amount;

            if (partial_exec_offer.amount < 0)
            {
                std::printf("partial_exec_offer.amount = %" PRId64 "\n",
                            partial_exec_offer.amount);
                std::printf("relevant_thunks.partial_exec_amount was %" PRId64 "\n",
                            relevant_thunks[i].partial_exec_amount);
                throw std::runtime_error(
                    "invalid partial exec offer leftover amount");
            }

            // std::printf("producing dbval for modified offer\n");

            if (partial_exec_offer.amount > 0)
            {
                auto modified_offer_buf = xdr::xdr_to_opaque(partial_exec_offer);
                dbval modified_offer = dbval{
                    modified_offer_buf.data(), modified_offer_buf.size()
                }; // xdr_to_dbval(partial_exec_offer);

                try
                {
                    wtx.put(get_data_dbi(), &partial_exec_key, &modified_offer);
                }
                catch (...)
                {
                    std::printf(
                        "failed to insert partial exec offer to offer lmdb\n");
                    throw;
                }
            } else
            {
                // partial_exec_offer.amount = 0
                wtx.del(get_data_dbi(), partial_exec_key);
            }
        }

        if (print)
            std::printf("phase 5\n");
        // finally, clear the partial exec offers, if they clear
        for (uint32_t i = 0; i < relevant_thunks.size(); i++)
        {

            if (relevant_thunks[i].current_block_number > current_block_number)
            {
                throw std::runtime_error("impossible!");
                // again ignore inflight thunks
                continue;
            }

            if (print)
                std::printf("phase 5 i %" PRIu32 " %" PRIu64 "\n",
                            i,
                            relevant_thunks[i].current_block_number);

            if (!relevant_thunks[i].get_exists_partial_exec())
            {
                continue;
            }

            for (uint32_t future = i + 1; future < relevant_thunks.size(); future++)
            {
                if (relevant_thunks[future].current_block_number
                    > current_block_number)
                {
                    throw std::runtime_error("impossible!!!");
                }

                if (print)
                    std::printf("continuing to future %" PRIu32 " %" PRIu64 "\n",
                                future,
                                relevant_thunks[future].current_block_number);
                if (relevant_thunks[i].partial_exec_key
                    < relevant_thunks[future].partial_exec_key)
                {

                    // strictly less than 0 - that is, round i's key is strictly
                    // less than round future's, so i's partial exec offer then
                    // fully clears in future.
                    //  We already took care of the 0 case in the preceding loop.
                    auto partial_exec_key_bytes
                        = relevant_thunks[i].partial_exec_key.template get_bytes_array<get_bytes_array_t>();
                    dbval partial_exec_key{
                        partial_exec_key_bytes
                    }; //(relevant_thunks[i].partial_exec_key.data(),
                       //MerkleWorkUnit::WORKUNIT_KEY_LEN);
                    wtx.del(get_data_dbi(), partial_exec_key);
                }
            }
        }
        if (print)
            std::printf("done saving workunit\n");
    }


    auto garbage = std::make_unique<ThunkGarbage<trie_t>>();

    for (auto& thunk : relevant_thunks)
    {
        garbage->add(thunk.cleared_offers
                         .dump_contents_for_detached_deletion_and_clear());
    }

    // commit_wtxn(wtx, current_block_number);

    return garbage;
}

OrderbookLMDB::OrderbookLMDB(OfferCategory const& category,
                             OrderbookManagerLMDB& manager_lmdb)
    : SharedLMDBInstance(manager_lmdb.get_base_instance(category))
    , thunks()
    , mtx(std::make_unique<std::mutex>())
{}

OrderbookLMDB::thunk_t&
OrderbookLMDB::add_new_thunk_nolock(uint64_t current_block_number)
{
    if (thunks.size())
    {
        // changed to drop strict sequentiality requirement
        if (thunks.back().current_block_number + 1 > current_block_number)
        {
            throw std::runtime_error("thunks in the wrong order!");
        }
    }
    thunks.emplace_back(current_block_number);
    return thunks.back();
}

} // namespace speedex
