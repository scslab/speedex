#include "hotstuff/protocol/hotstuff_server.h"

namespace hotstuff {

void
HotstuffProtocolHandler::vote(std::unique_ptr<VoteMessage> v)
{
	queue.validate_and_add_event(
		NetEvent(
			VoteNetEvent(std::move(v))));
}

void
HotstuffProtocolHandler::propose(std::unique_ptr<ProposeMessage> p)
{
	queue.validate_and_add_event(
		NetEvent(
			ProposalNetEvent(std::move(p))));
}

HotstuffProtocolServer::HotstuffProtocolServer(NetworkEventQueue& queue)
	: handler(queue)
	, ps()
	, protocol_listener(ps, xdr::tcp_listen(HOTSTUFF_PROTOCOL_PORT, AF_INET), false, xdr::session_allocator<void>())
	{
		protocol_listener.register_service(handler);
		
		std::thread([this] {ps.run();}).detach();
	}

} /* hotstuff */