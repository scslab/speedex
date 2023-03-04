/**
 * SPEEDEX: A Scalable, Parallelizable, and Economically Efficient Decentralized Exchange
 * Copyright (C) 2023 Geoffrey Ramseyer

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
	, listener(ps, xdr::tcp_listen(EXPERIMENT_CONTROL_PORT, AF_INET), false, xdr::session_allocator<void>())
	{
		listener.register_service(server);

		std::thread th([this] {
			while(!start_shutdown)
			{
				ps.poll(1000);
			}
			std::lock_guard lock(mtx);
			ps_is_shutdown = true;
			cv.notify_all();
		});

		th.detach();
	}

void
ExperimentController::await_pollset_shutdown()
{
	auto done_lambda = [this] () -> bool {
		return ps_is_shutdown;
	};

	std::unique_lock lock(mtx);
	if (!done_lambda()) {
		cv.wait(lock, done_lambda);
	}
	std::printf("shutdown happened\n");
}

} /* speedex */
