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

/*! \file speedex_management_structures.h

Store all of the data structures used in Speedex in one convenient
wrapper class.
*/

#include "header_hash/block_header_hash_map.h"

#include "memory_database/memory_database.h"

#include "modlog/account_modification_log.h"

#include "orderbook/orderbook_manager.h"

#include "price_computation/lp_solver.h"
#include "price_computation/normalization_rolling_average.h"
#include "price_computation/tatonnement_oracle.h"

#include "speedex/approximation_parameters.h"
#include "speedex/speedex_runtime_configs.h"

namespace speedex {

//! All of the data structures involved in running a SPEEDEX instance.
struct SpeedexManagementStructures {
	MemoryDatabase db;
	OrderbookManager orderbook_manager;
	AccountModificationLog account_modification_log;
	BlockHeaderHashMap block_header_hash_map;
	ApproximationParameters approx_params;

	const SpeedexRuntimeConfigs configs;

	//! Open all of the lmdb environment instances in Speedex.
	//! LMDB environments are opened before lmdb databases.
	void open_lmdb_env();

	//! Create all of the lmdb instances for Speedex.
	//! Will throw error if databases already exist.
	void create_lmdb();

	//! Open all off the LMDB instances in SPEEDEX
	void open_lmdb();

	//! Initialize speedex with a given number of assets and target
	//! approximation bounds
	SpeedexManagementStructures(
		uint16_t num_assets, 
		ApproximationParameters approx_params,
		SpeedexRuntimeConfigs configs)
		: db()
		, orderbook_manager(num_assets)
		, account_modification_log()
		, block_header_hash_map()
		, approx_params(approx_params)
		, configs(configs) {}
};


//! All of the data structures involved in running Tatonnement.
struct TatonnementManagementStructures {
	LPSolver lp_solver;
	TatonnementOracle oracle;
	NormalizationRollingAverage rolling_averages;

	//! Init tatonnment objects using supplied speedex objects.
	TatonnementManagementStructures(
		OrderbookManager& orderbook_manager)
		: lp_solver(orderbook_manager)
		, oracle(orderbook_manager, lp_solver)
		, rolling_averages(
			orderbook_manager.get_num_assets()) {}
};

} /* speedex */