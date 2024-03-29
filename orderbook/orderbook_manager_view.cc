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

#include "orderbook/orderbook_manager_view.h"

#include "orderbook/orderbook_manager.h"

namespace speedex
{

// LoadLMDBManagerView
LoadLMDBManagerView::LoadLMDBManagerView(uint64_t current_block_number,
                                         OrderbookManager& main_manager)
    : current_block_number(current_block_number)
    , main_manager(main_manager)
{}

void
LoadLMDBManagerView::add_offers(int idx, trie_t&& trie)
{
    if (main_manager.get_persisted_round_number(idx) < current_block_number)
    {

        main_manager.add_offers(idx, std::move(trie));
    }
}

std::optional<Offer>
LoadLMDBManagerView::mark_for_deletion(int idx, const prefix_t& key)
{
    if (main_manager.get_persisted_round_number(idx) < current_block_number)
    {

        return main_manager.mark_for_deletion(idx, key);
    }
    // Again, problem if memory database does not already reflect
    // the quantity of the asset that would normally be refunded
    // upon offer cancellation.
    return Offer();
}

void
LoadLMDBManagerView::unmark_for_deletion(int idx, const prefix_t& key)
{
    if (main_manager.get_persisted_round_number(idx) < current_block_number)
    {

        main_manager.unmark_for_deletion(idx, key);
    }
}

unsigned int
LoadLMDBManagerView::get_num_orderbooks() const
{
    return main_manager.get_num_orderbooks();
}

int
LoadLMDBManagerView::get_num_assets() const
{
    return main_manager.get_num_assets();
}

int
LoadLMDBManagerView::look_up_idx(const OfferCategory& id) const
{
    return main_manager.look_up_idx(id);
}

void 
ProcessingSerialManager::undelete_offer(
    const int idx, 
    const Price min_price, 
    const AccountID owner, 
    const uint64_t offer_id) {
    
    generate_orderbook_trie_key(
        min_price, owner, offer_id, BaseSerialManager::key_buf);
    
    BaseSerialManager<OrderbookManager>::main_manager
        .unmark_for_deletion(idx, key_buf);
}

void 
ProcessingSerialManager::unwind_add_offer(int idx, const Offer& offer) {
    generate_orderbook_trie_key(offer, key_buf);
    new_offers.at(idx).perform_deletion(key_buf);
}

} // namespace speedex