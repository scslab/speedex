#pragma once

#include "hotstuff/network_event_queue.h"

#include "rpc/rpcconfig.h"

#include "xdr/hotstuff.h"

#include <xdrpp/pollset.h>
#include <xdrpp/srpc.h>

namespace hotstuff {

class HotstuffProtocolHandler {

	NetworkEventQueue& queue;

public:

	using rpc_interface_type = HotstuffProtocolV1;

	HotstuffProtocolHandler(NetworkEventQueue& queue)
		: queue(queue)
		{}

	//rpc methods

	void vote(std::unique_ptr<VoteMessage> v);
	void propose(std::unique_ptr<ProposeMessage> p);
};

class HotstuffProtocolServer {

	HotstuffProtocolHandler handler;
	xdr::pollset ps;

	xdr::srpc_tcp_listener<> protocol_listener;

public:

	HotstuffProtocolServer(NetworkEventQueue& queue);
};


} /* hotstuff */