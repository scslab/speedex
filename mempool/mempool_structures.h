#pragma once

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

#include "mempool/mempool.h"
#include "mempool/mempool_cleaner.h"
#include "mempool/mempool_transaction_filter.h"

namespace speedex {

class MemoryDatabase;

class MempoolStructures {

public:
	// be sure to lock mempool if necessary
	Mempool mempool;
private:
	MempoolCleaner background_cleaner;
	MempoolFilterExecutor filter;
public:

	MempoolStructures(const MemoryDatabase& db, size_t target_chunk_size, size_t max_mempool_size)
		: mempool(target_chunk_size, max_mempool_size)
		, background_cleaner(mempool)
		, filter(db, mempool)
		{}
	
	void pre_validation_stop_background_filtering() {
		filter.stop_filter();
		background_cleaner.do_mempool_cleaning();
	}

	float post_validation_cleanup() {
		float clean_time = background_cleaner.wait_for_mempool_cleaning_done();
		mempool.push_mempool_buffer_to_mempool();
		filter.start_filter();
		return clean_time;
	}

	void pre_production_stop_background_filtering() {
		filter.stop_filter();
		mempool.push_mempool_buffer_to_mempool();
	}

	void during_production_post_tx_select_start_cleaning() {
		background_cleaner.do_mempool_cleaning();
	}

	float post_production_cleanup() {
		float clean_time = background_cleaner.wait_for_mempool_cleaning_done();
		return clean_time;
	}
};

} /* speedex */
