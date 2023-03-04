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

#include "overlay/overlay_server.h"

#include "mempool/mempool.h"

#include "config/replica_config.h"

#include "xdr/transaction.h"

#include "utils/debug_macros.h"

namespace speedex
{

using hotstuff::ReplicaConfig;
using hotstuff::ReplicaID;

std::unique_ptr<uint64_t>
OverlayHandler::mempool_size()
{
    return std::make_unique<uint64_t>(mempool.total_size());
}

void
OverlayHandler::log_batch_receipt(ReplicaID source, uint32_t batch_num)
{
    auto it = max_seen_batch_nums.find(source);
    if (it == max_seen_batch_nums.end())
    {
        return;
    }
    it->second
        = std::max(it->second.load(std::memory_order_relaxed), batch_num);
}

void
OverlayHandler::forward_txs(std::unique_ptr<ForwardingTxs> txs,
                            std::unique_ptr<uint32_t> tx_batch_num,
                            std::unique_ptr<ReplicaID> sender)
{

    try
    {
        log_batch_receipt(*sender, *tx_batch_num);
        xdr::xvector<SignedTransaction> blk;
        xdr::xdr_from_opaque(*txs, blk);

        OVERLAY_INFO("got %lu new txs for mempool, cur size %lu",
                     blk.size(),
                     mempool.total_size());

        mempool.chunkify_and_add_to_mempool_buffer(std::move(blk));
    }
    catch (...)
    {
        return;
    }
}

uint32_t
OverlayHandler::get_min_max_seen_batch_nums() const
{
    uint32_t min_max = UINT32_MAX;
    for (auto const& kv : max_seen_batch_nums)
    {
        min_max = std::min(kv.second.load(std::memory_order_relaxed), min_max);
    }
    return min_max;
}

OverlayServer::OverlayServer(Mempool& mempool,
                             const ReplicaConfig& config,
                             ReplicaID self_id)
    : handler(mempool, config)
    , ps()
    , overlay_listener(ps,
                       xdr::tcp_listen(static_cast<const ReplicaInfo&>(
                                           config.get_info(self_id))
                                           .overlay_port.c_str(),
                                       AF_INET),
                       false,
                       xdr::session_allocator<void>())
{
    overlay_listener.register_service(handler);

    std::thread th([this] {
        while (!start_shutdown)
        {
            ps.poll(1000);
        }
        std::lock_guard lock(mtx);
        ps_is_shutdown = true;
        cv.notify_all();
    });

    th.detach();
}

void
OverlayServer::await_pollset_shutdown()
{
    auto done_lambda = [this]() -> bool { return ps_is_shutdown; };

    std::unique_lock lock(mtx);
    if (!done_lambda())
    {
        cv.wait(lock, done_lambda);
    }
    std::printf("shutdown happened\n");
}

} // namespace speedex
