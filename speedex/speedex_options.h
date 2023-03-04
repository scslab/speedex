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
	uint32_t block_size;

	// operational parameters
	size_t persistence_frequency;
	size_t mempool_target;
	size_t mempool_chunk;

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