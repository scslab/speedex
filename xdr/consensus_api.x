
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


namespace speedex {

typedef string suffixname<>;

program HotstuffVMControl {
	version HotstuffVMControlV1 {
		void signal_breakpoint(void) = 1;
		void write_measurements(void) = 2;
		ExperimentResultsUnion get_measurements(void) = 3;
		uint32 experiment_is_done(void) = 4;
		void send_producer_is_done_signal(void) = 5;
		uint64 get_speedex_block_height(void) = 6;
		suffixname get_measurement_name_suffix(void) = 7;
	} = 1;
} = 0x11111113;

} /* speedex */
