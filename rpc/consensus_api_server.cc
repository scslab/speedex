#include "rpc/consensus_api_server.h"

namespace speedex {

ConsensusApiServer::ConsensusApiServer(SpeedexNode& main_node)
	: transfer_server(main_node)
	, req_server(main_node)
	, control_server(main_node)
	, ps()
	, bt_listener(ps, xdr::tcp_listen(BLOCK_FORWARDING_PORT, AF_INET), false, xdr::session_allocator<void>())
	, req_listener(ps, xdr::tcp_listen(FORWARDING_REQUEST_PORT, AF_INET), false, xdr::session_allocator<rpc_sock_ptr>())
	, control_listener(ps, xdr::tcp_listen(SERVER_CONTROL_PORT, AF_INET), false, xdr::session_allocator<void>()) {
		bt_listener.register_service(transfer_server);
		req_listener.register_service(req_server);
		control_listener.register_service(control_server);

		std::thread th([this] {ps.run();});
		th.detach();
	}

} /* speedex */
