#include "speedex/speedex_options.h"

#include <stdexcept>

#include <libfyaml.h>

namespace speedex {


void SpeedexOptions::parse_options(const char* filename) {

	struct fy_document* fyd = fy_document_build_from_file(NULL, filename);
	
	if (fyd == NULL) {
		throw std::runtime_error(
			"could not open file (did you forget to type .yaml?)");
	}

	int count = 0;
	count += fy_document_scanf(
		fyd,
		"/protocol/tax_rate %u "
		"/protocol/smooth_mult %u "
		"/protocol/num_assets %u",
		&tax_rate, &smooth_mult, &num_assets);

	count += fy_document_scanf(
		fyd,
		"/speedex-node/persistence_frequency %lu",
		&persistence_frequency);

	if (count != 4) {
		throw std::runtime_error("failed to parse options yaml");
	}
}


void SpeedexOptions::print_options() {
	std::printf("tax_rate=%u smooth_mult=%u num_assets=%u\n", 
		tax_rate, smooth_mult, num_assets);
}

} /* speedex */
