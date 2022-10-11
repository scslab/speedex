#include "automation/experiment_control.h"

#include "rpc/rpcconfig.h"

#include "speedex/vm/speedex_vm.h"

#include "utils/debug_macros.h"

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

std::unique_ptr<uint32_t> 
HotstuffVMControl_server::experiment_is_done() const {
	if (experiment_done_flag) {
		return std::make_unique<uint32_t>(1);
	}
	if (vm -> experiment_is_done()) {
		return std::make_unique<uint32_t>(1);
	}
	return std::make_unique<uint32_t>(0);
}
void 
HotstuffVMControl_server::send_producer_is_done_signal()
{
	experiment_done_flag = true;
}

std::unique_ptr<uint64_t> 
HotstuffVMControl_server::get_speedex_block_height() {
	return std::make_unique<uint64_t>(vm -> get_lead_block_height());
}


void
HotstuffVMControl_server::wait_for_breakpoint_signal() {
	auto done_lambda = [this] () -> bool {
		return bp_signalled;
	};

	std::unique_lock lock(mtx);
	if (!done_lambda()) {
		LOG("waiting for experiment breakpoint");
		cv.wait(lock, done_lambda);
	}
	bp_signalled = false;
}

ExperimentController::ExperimentController(std::shared_ptr<SpeedexVM> vm, std::string measurement_name_suffix)
	: server(vm, measurement_name_suffix)
	, ps()
	, listener(xdr::srpc_tcp_listener<>(ps, xdr::tcp_listen(EXPERIMENT_CONTROL_PORT, AF_INET), false, xdr::session_allocator<void>()))
	{
		listener.register_service(server);

		std::thread th([this] {
			while(!start_shutdown.test())
			{
				ps.poll(1000);
			}
			std::printf("done run\n");
			std::lock_guard lock(mtx);
			ps_is_shutdown = true;
			cv.notify_all();
		});

		th.detach();
	}

} /* speedex */
