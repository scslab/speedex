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

#include "speedex/speedex_options.h"

#include <stdexcept>
#include <cinttypes>

#include <libfyaml.h>
#include "utils/yaml.h"

namespace speedex {


void SpeedexOptions::parse_options(const char* filename) {

	yaml fyd(filename);
	
	if (!fyd) {
		throw std::runtime_error(
			"could not open file (did you forget to type .yaml?)");
	}

	int count = 0;
	count += fy_document_scanf(
		fyd.get(),
		"/protocol/tax_rate %hhu "
		"/protocol/smooth_mult %hhu "
		"/protocol/num_assets %hu "
		"/protocol/block_size %u",
		&tax_rate, &smooth_mult, &num_assets, &block_size);

	count += fy_document_scanf(
		fyd.get(),
		"/speedex-node/persistence_frequency %lu "
		"/speedex-node/mempool_target %lu "
		"/speedex-node/mempool_chunk %lu",
		&persistence_frequency, &mempool_target, &mempool_chunk);

	if (count != 7) {
		throw std::runtime_error("failed to parse options yaml");
	}
}


void SpeedexOptions::print_options() {
	std::printf("===== SPEEDEX OPTIONS =====\n");
	std::printf("tax rate    %" PRIu8 "\n", tax_rate);
	std::printf("smooth mult %" PRIu8 "\n", smooth_mult);
	std::printf("num assets  %" PRIu16 "\n", num_assets);
	std::printf("block size  %" PRIu32 "\n", block_size);
	std::printf("mp target   %u\n", mempool_target);
	std::printf("mp chunk sz %u\n", mempool_chunk);
}

} /* speedex */
