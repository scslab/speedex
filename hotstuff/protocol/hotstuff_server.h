#pragma once

#include "rpc/rpcconfig.h"

#include "xdr/hotstuff.h"

#include <xdrpp/pollset.h>
#include <xdrpp/srpc.h>

namespace hotstuff {

class ReplicaConfig;
class NetworkEventQueue;

class HotstuffProtocolHandler {

	NetworkEventQueue& queue;

	const ReplicaConfig& config;

public:

	using rpc_interface_type = HotstuffProtocolV1;

	HotstuffProtocolHandler(NetworkEventQueue& queue, const ReplicaConfig& config)
		: queue(queue)
		, config(config)
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

	HotstuffProtocolServer(NetworkEventQueue& queue, const ReplicaConfig& config);
};


} /* hotstuff */