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


public:
	using rpc_interface_type = HotstuffVMControlV1;

	HotstuffVMControl_server(std::shared_ptr<SpeedexVM> vm)
		: vm(vm)
		, bp_signalled(false)
		, mtx()
		, cv()
		, experiment_done_flag(false)
		{}

	//rpc methods
	void signal_breakpoint();
	void write_measurements();
	std::unique_ptr<ExperimentResultsUnion> get_measurements();
	std::unique_ptr<uint32_t> experiment_is_done() const;
	void send_producer_is_done_signal(); // receives this signal
	std::unique_ptr<uint64_t> get_speedex_block_height();

	//non-rpc methods
	void wait_for_breakpoint_signal();
};

class ExperimentController {
	HotstuffVMControl_server server;

	xdr::pollset ps;
	xdr::srpc_tcp_listener<> listener;
public:

	ExperimentController(std::shared_ptr<SpeedexVM> vm);

	void wait_for_breakpoint_signal() {
		server.wait_for_breakpoint_signal();
	}

	bool producer_is_done_signal_was_received() const {
		return *server.experiment_is_done();
	}
};



} /* speedex */
