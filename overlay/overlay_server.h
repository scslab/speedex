#pragma once

#include "xdr/overlay.h"

#include <xdrpp/pollset.h>
#include <xdrpp/srpc.h>

namespace speedex {

class Mempool;

class OverlayHandler {

	Mempool& mempool;

public:

	OverlayHandler(Mempool& mempool)
		: mempool(mempool)
		{}

	using rpc_interface_type = OverlayV1;

	std::unique_ptr<uint64_t> mempool_size();

	void forward_txs(std::unique_ptr<ForwardingTxs> txs);
};

class OverlayServer {
	OverlayHandler handler;

	xdr::pollset ps;
	xdr::srpc_tcp_listener<> overlay_listener;

public:

	OverlayServer(Mempool& mempool);

};


} /* speedex */
