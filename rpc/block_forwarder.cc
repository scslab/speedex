#include "rpc/block_forwarder.h"

#include "utils/debug_macros.h"

namespace speedex {

SerializedBlock serialize_block(const AccountModificationBlock& block) {
	SignedTransactionList list;
	for (auto const& log : block) {
		list.insert(
			list.end(), 
			log.new_transactions_self.begin(), 
			log.new_transactions_self.end());
	}
	return xdr::xdr_to_opaque(list);
}

void 
BlockForwarder::send_block_(const HashedBlock& header, const SerializedBlock& serialized_data) {
	BLOCK_INFO(
		"sending block number %lu to %lu clients", 
		header.block.blockNumber, 
		forwarding_targets.size());

	for (auto& client : forwarding_targets) {
		if (!client) {
			BLOCK_INFO("Lost connection to a client!!!");
		} else {
			client->send_block(header, serialized_data);
		}
	}
	BLOCK_INFO("done sending block %lu", header.block.blockNumber);
}

void 
BlockForwarder::send_block_(const HashedBlock& header, const AccountModificationBlock& block) {
	auto serialized_data = serialize_block(block);
	send_block_(header, serialized_data);
}

void 
BlockForwarder::shutdown_target_connections() {
	wait_for_async_task();

	std::lock_guard lock(mtx);
	forwarding_targets.clear();
	sockets.clear();

	for (auto host : shutdown_notification_hosts) {
		auto fd = xdr::tcp_connect(host.c_str(), SERVER_CONTROL_PORT);
		if (fd) {
			auto client = xdr::srpc_client<ExperimentControlV1>(fd.get());
			client.signal_upstream_finish();
		}
	}
	num_forwarding_targets = 0;
}

void 
BlockForwarder::add_forwarding_target(const std::string& hostname) {
	std::lock_guard lock(mtx);
	num_forwarding_targets ++; 
	BLOCK_INFO("connecting to %s", hostname.c_str());

	auto fd = xdr::tcp_connect(hostname.c_str(), BLOCK_FORWARDING_PORT);

	auto client = std::make_unique<forwarding_client_t>(fd.get());
	
	forwarding_targets.emplace_back(std::move(client));
	sockets.emplace_back(std::move(fd));

	shutdown_notification_hosts.push_back(hostname);
	BLOCK_INFO("done connecting to forwarding target");
}

void
BlockForwarder::request_forwarding_from(const std::string& hostname) {
	BLOCK_INFO("requesting block forwarding from %s", hostname.c_str());

	auto fd = xdr::tcp_connect(hostname.c_str(), FORWARDING_REQUEST_PORT);

	request_client_t req_client{fd.get()};

	if (!req_client.request_forwarding()) {
		throw std::runtime_error("unable to get request block forwarding");
	}
}

void 
BlockForwarder::run() {
	BLOCK_INFO("Starting block forwarder thread");
	while (true) {
		std::unique_lock lock(mtx);

		if ((!done_flag) && (!exists_work_to_do())) {
			cv.wait(lock, [this] () { return done_flag || exists_work_to_do();});
		}
		if (done_flag) return;
		if (block_to_send) {
			send_block_(header_to_send, *block_to_send);
			block_to_send = nullptr;
		}
		if (block_to_send2) {
			send_block_(header_to_send, *block_to_send2);
			block_to_send2 = nullptr;
		}

		cv.notify_all();
	}
}

void 
BlockForwarder::send_block(
	const HashedBlock& header, 
	std::unique_ptr<AccountModificationBlock> block)
{
	wait_for_async_task();
	std::lock_guard lock(mtx);
	block_to_send = std::move(block);
	header_to_send = header;
	cv.notify_all();
}

void 
BlockForwarder::send_block(
	const HashedBlock& header, 
	std::unique_ptr<SerializedBlock> block)
{
	wait_for_async_task();
	std::lock_guard lock(mtx);
	block_to_send2 = std::move(block);
	header_to_send = header;
	cv.notify_all();
}

} /* namespace speedex */