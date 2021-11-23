#include "automation/experiment_control.h"

#include "rpc/rpcconfig.h"

#include "speedex/vm/speedex_vm.h"

namespace speedex {

void 
HotstuffVMControl_server::signal_breakpoint() {
	bp_signalled = true;
	cv.notify_one();
}

void 
HotstuffVMControl_server::write_measurements() {
	vm -> write_measurements();
}

std::unique_ptr<ExperimentResultsUnion>
HotstuffVMControl_server::get_measurements() {
	return std::make_unique<ExperimentResultsUnion>(vm -> get_measurements());
}

void
HotstuffVMControl_server::wait_for_breakpoint_signal() {
	auto done_lambda = [this] () -> bool {
		return bp_signalled;
	};

	std::unique_lock lock(mtx);
	if (!done_lambda()) {
		cv.wait(lock, done_lambda);
	}
	bp_signalled = false;
}

ExperimentController::ExperimentController(std::shared_ptr<SpeedexVM> vm)
	: server(vm)
	, ps()
	, listener(ps, xdr::tcp_listen(EXPERIMENT_CONTROL_PORT, AF_INET), false, xdr::session_allocator<void>())
	{
		listener.register_service(server);

		std::thread th([this] {ps.run();});
		th.detach();
	}

} /* speedex */
