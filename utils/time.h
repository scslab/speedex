#pragma once

#include <chrono>

/*!  \file Minor utility functions for measuring time */

namespace speedex {

namespace {
	using time_point = std::chrono::time_point<std::chrono::steady_clock>;
}

//! \fn difference between two time measurements.
inline static double 
time_diff(const time_point& start, const time_point& end) {
	return ((double) std::chrono::duration_cast<std::chrono::microseconds>(
						end-start)
					.count())
				/ 1000000;
}

inline static time_point 
init_time_measurement() {
	return std::chrono::steady_clock::now();
}

/*! \fn measures time since \a prev_measurement and sets
	\a prev_measurement to the current time.
*/
inline static double 
measure_time(time_point& prev_measurement) {
	auto new_measurement = std::chrono::steady_clock::now();
	auto out = time_diff(prev_measurement, new_measurement);
	prev_measurement = new_measurement;
	return out;
}

inline static double 
measure_time_from_basept(const time_point& basept) {
	auto new_measurement = init_time_measurement();
	return time_diff(basept, new_measurement);
}

}