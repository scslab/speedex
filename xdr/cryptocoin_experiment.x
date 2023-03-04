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
	OfferCategory category;
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