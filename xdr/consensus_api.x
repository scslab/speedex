
#if defined(XDRC_HH) || defined(XDRC_SERVER)
%#include "xdr/block.h"
%#include "xdr/experiments.h"
#endif

#if defined(XDRC_PXDI)
%from types_xdr cimport *
%from block_xdr cimport *
%from experiments_xdr cimport *
#endif

#if defined(XDRC_PXD)
%from types_xdr cimport *
%from block_xdr cimport *
%from experiments_xdr cimport *
%from consensus_api_includes cimport *
#endif

#if defined(XDRC_PYX)
%from types_xdr cimport *
%from block_xdr cimport *
%from experiments_xdr cimport *
%from consensus_api_includes cimport *
#endif


namespace edce {
	
program BlockTransfer{
	version BlockTransferV1 {
		void send_block(HashedBlock, SerializedBlock) = 1;
	} = 1;
} = 0x10734299;

program BlockAcknowledge {
	version BlockAcknowledgeV1 {
		void ack_block(uint64) = 1;
	} = 1;
} = 0x11111111;

typedef string hostname<>;

program RequestBlockForwarding {
	version RequestBlockForwardingV1 {
		void request_forwarding(hostname) = 1;
	} = 1;
} = 0x11111112;

program ExperimentControl {
	version ExperimentControlV1 {
		void write_measurements(void) = 1;
		void signal_start(void) = 2;
		ExperimentResultsUnion get_measurements(void) = 3;
		uint32 is_running(void) = 4;
		uint32 is_ready_to_start(void) = 5;
		void signal_upstream_finish(void) = 6;
	} = 1;
} = 0x11111113;

}
