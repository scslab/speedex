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