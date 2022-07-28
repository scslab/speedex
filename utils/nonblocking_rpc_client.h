#pragma once

#include "config/replica_config.h"
#include "hotstuff/config/replica_config.h"

#include "utils/async_worker.h"
#include "utils/debug_macros.h"

#include <memory>

#include <xdrpp/socket.h>
#include <xdrpp/srpc.h>

namespace speedex {

template<typename client_t>
class NonblockingRpcClient : public AsyncWorker {

	xdr::unique_sock socket;

protected:

	using AsyncWorker::mtx;
	using AsyncWorker::cv;

	hotstuff::ReplicaInfo info;

	//not threadsafe to access outside of a try_action call
	std::unique_ptr<client_t> client;

private:
	//not threadsafe
	void open_connection();
	void try_open_connection();
	void wait_for_try_open_connection();
	bool connection_is_open();
	void clear_connection();

protected:
	//override to call event handlers
	virtual void on_connection_clear() {}
	virtual void on_connection_open() {}

	void wait();

	virtual const char* get_service() const = 0;

	NonblockingRpcClient(hotstuff::ReplicaInfo const& info)
		: AsyncWorker()
		, socket()
		, info(info)
		, client(nullptr)
		{}

	// These calls are NOT threadsafe
	template<typename ReturnType>
	std::unique_ptr<ReturnType> try_action(auto lambda);
	bool try_action_void(auto lambda);
};


template<typename client_t>
void
NonblockingRpcClient<client_t>::try_open_connection()
{
	try {
		open_connection();
	} catch (...)
	{
		INFO("failed to open connection on rid=%d", info.id);
		clear_connection();
	}
	on_connection_open();
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

template<typename client_t>
template<typename ReturnType>
std::unique_ptr<ReturnType>
NonblockingRpcClient<client_t>::try_action(auto lambda)
{
	wait_for_try_open_connection();
	std::unique_ptr<ReturnType> out = nullptr;
	try {
		out = lambda();
	} catch (...) {
		out = nullptr;
		clear_connection();
	}
	return out;
}

template<typename client_t>
bool
NonblockingRpcClient<client_t>::try_action_void(auto lambda)
{
	wait_for_try_open_connection();
	bool res = true;
	try {
		lambda();
	} catch(...) {
		res = false;
		clear_connection();
	}
	return res;
}

} /* speedex */
