#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <xdrpp/arpc.h>
#include <xdrpp/msgsock.h>
#include <xdrpp/srpc.h>

#include <mutex>
#include <condition_variable>

#include "xdr/consensus_api.h"
#include "speedex/speedex_node.h"

namespace speedex {

class ValidatorCaller : public AsyncWorker {
  using AsyncWorker::mtx;
  using AsyncWorker::cv;

  SpeedexNode& main_node;

  using work_type = std::pair<HashedBlock, std::unique_ptr<SerializedBlock>>;

  std::vector<work_type> blocks;

  bool in_progress = false;

  bool exists_work_to_do() override final {
    return blocks.size() != 0 || in_progress;
  }

  void run();

public:
  ValidatorCaller(SpeedexNode& main_node) 
    : AsyncWorker()
    , main_node(main_node) {
      start_async_thread([this] { run();});
    }

  ~ValidatorCaller() {
    wait_for_async_task();
    end_async_thread();
  }
  void validate_block(const HashedBlock& new_header, std::unique_ptr<SerializedBlock>&& new_block);
};

class BlockTransferV1_server {

	SpeedexNode& main_node;
  ValidatorCaller caller;
public:
  using rpc_interface_type = BlockTransferV1;

  BlockTransferV1_server(SpeedexNode& main_node) 
    : main_node(main_node)
    , caller(main_node) {};

  void send_block(const HashedBlock &header, std::unique_ptr<SerializedBlock> block);

  void wait_until_block_buffer_empty() {
    caller.wait_for_async_task();
  }
};

struct rpc_sock_ptr {
  xdr::rpc_sock* ptr;

  std::string get_caller_ip() const;

};

class RequestBlockForwardingV1_server {

	SpeedexNode& main_node;
public:
  using rpc_interface_type = RequestBlockForwardingV1;
  
  RequestBlockForwardingV1_server(SpeedexNode& main_node) : main_node(main_node) {};

  std::unique_ptr<uint32_t>
  request_forwarding(rpc_sock_ptr* session);
};

class ExperimentControlV1_server {
  SpeedexNode& main_node;
  std::mutex wait_mtx;
  std::condition_variable wait_cv;
  bool wakeable = false;

  std::atomic<bool> experiment_finished = false;
  std::atomic<bool> experiment_ready_to_start = false;

  std::atomic<bool> upstream_finished = false;
public:
  using rpc_interface_type = ExperimentControlV1;
  
  ExperimentControlV1_server(SpeedexNode& main_node) : main_node(main_node) {};

  void write_measurements();

  void signal_start();

  std::unique_ptr<ExperimentResultsUnion> get_measurements();

  std::unique_ptr<unsigned int> is_running(); // returns a boolean : 0 if not running (done), 1 if running

  std::unique_ptr<unsigned int> is_ready_to_start(); //returns boolean : 1 if ready, 0 if not.

  void signal_upstream_finish();

  //not rpc
  void wait_for_start();
  void set_experiment_done() {
    experiment_finished = true;
  }
  void set_experiment_ready_to_start() {
    experiment_ready_to_start = true;
  }

  void wait_for_upstream_finish();
};

}
