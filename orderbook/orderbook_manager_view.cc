#include "orderbook/orderbook_manager_view.h"

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

} // namespace speedex