#if defined(XDRC_HH) || defined(XDRC_SERVER)
%#include "xdr/types.h"
%#include "xdr/transaction.h"
#endif

#if defined(XDRC_PXDI) || defined(XDRC_PXD)
%from types_includes cimport *
%from types_xdr cimport *
%from transaction_includes cimport *
%from cryptocoin_experiment_includes cimport *
#endif

#if defined(XDRC_PYX)
%from types_xdr cimport *
%from transaction_includes cimport *
%from cryptocoin_experiment_includes cimport *
#endif

namespace speedex
{

struct DateSnapshot {
	float volume;
	float price;
};

struct Cryptocoin {
	DateSnapshot snapshots<>;
	string name<>;
};

struct CryptocoinExperiment {
	Cryptocoin coins<>;
};

struct GeneratedCoinVolumeSnapshots {
	float coin_volumes<>;
};

struct GeneratedExperimentVolumeData {
	GeneratedCoinVolumeSnapshots snapshots<>;	
};

struct ExchangeOrderbookSnapshot {
	OfferCategory category;
	Offer offers<>;
};

struct CancelEvent {
	uint64 cancelledOfferId;
	Price cancelledOfferPrice;
};

union ExchangeEvent switch(uint32 v) {
	case 0:
		Offer newOffer;
	case 1:
		CancelEvent cancel;
};

struct ExchangeExperiment {
	ExchangeOrderbookSnapshot initial_snapshots<>;
	ExchangeEvent event_stream<>;
};


} /* speedex */