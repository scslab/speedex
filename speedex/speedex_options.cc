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
		"/protocol/tax_rate %hhu "
		"/protocol/smooth_mult %hhu "
		"/protocol/num_assets %hu "
		"/protocol/block_size %u",
		&tax_rate, &smooth_mult, &num_assets, &block_size);

	count += fy_document_scanf(
		fyd,
		"/speedex-node/persistence_frequency %lu "
		"/speedex-node/mempool_target %lu "
		"/speedex-node/mempool_chunk %lu",
		&persistence_frequency, &mempool_target, &mempool_chunk);

	if (count != 7) {
		throw std::runtime_error("failed to parse options yaml");
	}
	fy_document_destroy(fyd);
}


void SpeedexOptions::print_options() {
	std::printf("tax_rate=%u smooth_mult=%u num_assets=%u\n", 
		tax_rate, smooth_mult, num_assets);
}

} /* speedex */
