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

#include <cstdint>

/*! \file approximation_parameters.h

Store in one object the approximation parameter
targets for speedex operation.
*/

namespace speedex {

/*! Approximation bounds for speedex operation.
Values represented as negative logs.

i.e. the real tax rate (epsilon) is 2^-tax_rate.

smooth_mult (the "smoothness multiplier") refers
to the approximately optimal offer execution
parameter (mu)
*/
struct ApproximationParameters {
	//! -log_2 of transaction commission
	uint8_t tax_rate;
	//! -log_2 of approximately optimal 
	//! offer execution parameter
	uint8_t smooth_mult;
};
 
} /* speedex */