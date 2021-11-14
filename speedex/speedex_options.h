#pragma once

#include <cstddef>
#include <cstdint>

#include "speedex/approximation_parameters.h"

namespace speedex {

struct SpeedexOptions {

	// protocol parameters
	uint8_t tax_rate;
	uint8_t smooth_mult;
	uint16_t num_assets;

	// operational parameters
	size_t persistence_frequency;

	void parse_options(const char* configfile);

	void print_options();

	ApproximationParameters get_approx_params() const {
		return ApproximationParameters {
			.tax_rate = tax_rate,
			.smooth_mult = smooth_mult
		};
	}
};


} /* speedex */