#if defined(XDRC_HH) || defined(XDRC_SERVER)
%#include "xdr/types.h"
#endif

#if defined(XDRC_PXDI) || defined(XDRC_PXD)
%from types_includes cimport *
%from types_xdr cimport *
%from cryptocoin_experiment_includes cimport *
#endif

#if defined(XDRC_PYX)
%from types_xdr cimport *
%from cryptocoin_experiment_includes cimport *
#endif

namespace edce
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


} /* edce */