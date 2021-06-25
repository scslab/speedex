#pragma once
#include <cstddef>

namespace speedex {

struct SpeedexOptions {

	// protocol parameters
	unsigned int tax_rate;
	unsigned int smooth_mult;
	unsigned int num_assets;

	// operational parameters
	size_t persistence_frequency;

	void parse_options(const char* configfile);

	void print_options();
};


} /* speedex */