#pragma once

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
public:

	ExperimentController(std::shared_ptr<SpeedexVM> vm, std::string measurement_name_suffix = "");

	void wait_for_breakpoint_signal() {
		server.wait_for_breakpoint_signal();
	}

	bool producer_is_done_signal_was_received() const {
		return server.got_experiment_done_flag();
	}
};



} /* speedex */
