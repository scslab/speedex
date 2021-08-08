
#include "rpc/consensus_api.h"


#include "utils/debug_macros.h"

#include <xdrpp/marshal.h>

namespace speedex {

void
ValidatorCaller::validate_block(const HashedBlock& new_header, std::unique_ptr<SerializedBlock>&& new_block) {

  std::lock_guard lock(mtx);

  blocks.emplace_back(new_header, std::move(new_block));

  cv.notify_all();
}

void
ValidatorCaller::run() {
  BLOCK_INFO("starting validator caller async task thread");

  while(true) {
    std::unique_lock lock(mtx);
    if ((!done_flag) && (!exists_work_to_do())) {
      cv.wait(lock, [this] () {return done_flag || exists_work_to_do();});
    }

    if (done_flag) return;

    if (blocks.size() > 0) {

      auto [header, block] = std::move(blocks[0]);
      blocks.erase(blocks.begin());
      in_progress = true;
      lock.unlock();

      auto res = main_node.validate_block(header, std::move(block));
      if (!res) {
        BLOCK_INFO("block validation failed!!!");
      } else {
        BLOCK_INFO("block validation succeeded");
      }

      lock.lock();
      in_progress = false;
    }
    cv.notify_all();
  }
}

std::string 
rpc_sock_ptr::get_caller_ip() const {
  int fd = ptr -> ms_ -> get_sock().fd();

  struct sockaddr sa;

  socklen_t sval = sizeof(sa);

  getpeername(fd, &sa, &sval);

  struct sockaddr_in *addr_in = (struct sockaddr_in*) &sa;

  char* ip = inet_ntoa(addr_in->sin_addr);

  return std::string(ip);
}

void
BlockTransferV1_server::send_block(const HashedBlock &header, std::unique_ptr<SerializedBlock> block)
{
  BLOCK_INFO("got new block for header number %lu", header.block.blockNumber);

  caller.validate_block(header, std::move(block));
}


std::unique_ptr<uint32_t>
RequestBlockForwardingV1_server::request_forwarding(rpc_sock_ptr* session)
{
  auto ip_addr = session->get_caller_ip();
  BLOCK_INFO("adding forwarding target to my targets %s", ip_addr.c_str());
  main_node.get_block_forwarder().add_forwarding_target(ip_addr); 
  BLOCK_INFO("done adding forwarding target");
  return 1;
}

void
ExperimentControlV1_server::write_measurements()
{
	BLOCK_INFO("forcing measurements to be logged to disk");
	main_node.write_measurements(); 
}

void
ExperimentControlV1_server::signal_start()
{
  BLOCK_INFO("got signal to start experiment");
  std::unique_lock lock(wait_mtx);

  wakeable = true;
  wait_cv.notify_all();
}

void 
ExperimentControlV1_server::signal_upstream_finish() {
  BLOCK_INFO("got signal that upstream is done");
  std::unique_lock lock(wait_mtx);
  upstream_finished = true;
  wait_cv.notify_all();
}


std::unique_ptr<ExperimentResultsUnion>
ExperimentControlV1_server::get_measurements() {
  return std::make_unique<ExperimentResultsUnion>(main_node.get_measurements());
}

std::unique_ptr<unsigned int>
ExperimentControlV1_server::is_running() {
  return std::make_unique<unsigned int>(experiment_finished ? 0 : 1);
}

std::unique_ptr<unsigned int>
ExperimentControlV1_server::is_ready_to_start() {
  return std::make_unique<unsigned int>(experiment_ready_to_start ? 1 : 0);
}

void
ExperimentControlV1_server::wait_for_start() {
  BLOCK_INFO("waiting for experiment control signal");
  std::unique_lock lock(wait_mtx);
  if (wakeable) {
    wakeable = false;
    return;
  }
  wait_cv.wait(lock, [this] { return wakeable;});
  BLOCK_INFO("woke up from experiment control signal");
  wakeable = false;
}

void
ExperimentControlV1_server::wait_for_upstream_finish() {
  BLOCK_INFO("waiting for upstream finished signal");
  std::unique_lock lock(wait_mtx);
  if (upstream_finished) {
    return;
  }
  wait_cv.wait(lock, [this] { return upstream_finished.load();});
}



} /* speedex */
