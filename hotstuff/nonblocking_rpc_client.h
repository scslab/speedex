#pragma once

#include "hotstuff/replica_config.h"

#include "utils/async_worker.h"
#include "utils/debug_macros.h"

#include <memory>

#include <xdrpp/socket.h>
#include <xdrpp/srpc.h>

namespace hotstuff {

template<typename client_t>
class NonblockingRpcClient : public speedex::AsyncWorker {

	xdr::unique_sock socket;

protected:
	using speedex::AsyncWorker::mtx;
	using speedex::AsyncWorker::cv;

	ReplicaInfo info;

	std::unique_ptr<client_t> client;

	void open_connection();
	void try_open_connection();
	void wait_for_try_open_connection();
	bool connection_is_open();
	void clear_connection();
	void wait();

	virtual const char* get_service() const = 0;

	NonblockingRpcClient(ReplicaInfo const& info)
		: socket()
		, info(info)
		, client(nullptr)
		{}

	~NonblockingRpcClient()
	{
		wait_for_async_task();
		end_async_thread();
	}
};


template<typename client_t>
void
NonblockingRpcClient<client_t>::try_open_connection()
{
	try {
		open_connection();
	} catch (...)
	{
		HOTSTUFF_INFO("failed to open connection on rid=%d", info.id);
		clear_connection();
	}
}
 
template<typename client_t>
void
NonblockingRpcClient<client_t>::wait() {
	using namespace std::chrono_literals;
	std::this_thread::sleep_for(1000ms);
}

template<typename client_t>
void
NonblockingRpcClient<client_t>::wait_for_try_open_connection()
{
	if (connection_is_open() || done_flag)
	{
		return;
	}
	while(true) {
		try_open_connection();
		if (connection_is_open() || done_flag)
		{
			return;
		}
		wait();
	}
}

template<typename client_t>
bool
NonblockingRpcClient<client_t>::connection_is_open() {
	return client != nullptr;
}

template<typename client_t>
void
NonblockingRpcClient<client_t>::clear_connection() {
	client = nullptr;
	socket.clear();
}

template<typename client_t>
void
NonblockingRpcClient<client_t>::open_connection() 
{
	socket = info.tcp_connect(get_service());
	client = std::make_unique<client_t>(socket.get());
}


} /* hotstuff */