#if defined(XDRC_HH) || defined(XDRC_SERVER)
%#include "xdr/types.h"
%#include "xdr/transaction.h"
#endif

#if defined(XDRC_PXDI) || defined(XDRC_PXD)
%from types_xdr cimport *
%from transaction_xdr cimport *
%from ledger_includes cimport *
#endif

#if defined(XDRC_PYX)
%from types_xdr cimport *
%from transaction_xdr cimport *
%from ledger_includes cimport *
#endif
namespace edce {
	
struct TransactionSet {
	Transaction txs<>;
};

//add in all header stuff here



}