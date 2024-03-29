#pragma once

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

#include "xdr/consensus_api.h"

#include <xdrpp/srpc.h>
#include <xdrpp/pollset.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>

namespace speedex {

class SpeedexVM;

class HotstuffVMControl_server {

	std::shared_ptr<SpeedexVM> vm;

	std::atomic<bool> bp_signalled;
	std::mutex mtx;
	std::condition_variable cv;

	std::atomic<bool> experiment_done_flag;

	const std::string measurement_name_suffix;

public:
	using rpc_interface_type = HotstuffVMControlV1;

	HotstuffVMControl_server(std::shared_ptr<SpeedexVM> vm, std::string measurement_name_suffix)
		: vm(vm)
		, bp_signalled(false)
		, mtx()
		, cv()
		, experiment_done_flag(false)
		, measurement_name_suffix(measurement_name_suffix)
		{}

	//rpc methods
	void signal_breakpoint();
	void write_measurements();
	std::unique_ptr<ExperimentResultsUnion> get_measurements();
	std::unique_ptr<uint32_t> experiment_is_done() const;
	void send_producer_is_done_signal(); // receives this signal
	std::unique_ptr<uint64_t> get_speedex_block_height();
	std::unique_ptr<xdr::xstring<>> get_measurement_name_suffix()
	{
		return std::make_unique<xdr::xstring<>>(measurement_name_suffix);
	}

	//non-rpc methods
	void wait_for_breakpoint_signal();
	bool got_experiment_done_flag() const {
		return experiment_done_flag;
	}
};

class ExperimentController {
	HotstuffVMControl_server server;

	xdr::pollset ps;
	xdr::srpc_tcp_listener<> listener;

	bool ps_is_shutdown = false;
	std::atomic<bool> start_shutdown = false;
	std::mutex mtx;
	std::condition_variable cv;

	void await_pollset_shutdown();

public:

	ExperimentController(std::shared_ptr<SpeedexVM> vm, std::string measurement_name_suffix = "");

	~ExperimentController()
	{
		start_shutdown = true;
		await_pollset_shutdown();
	}

	void wait_for_breakpoint_signal() {
		server.wait_for_breakpoint_signal();
	}

	bool producer_is_done_signal_was_received() const {
		return server.got_experiment_done_flag();
	}


};



} /* speedex */
