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

namespace speedex {

//! All of the data structures involved in running a SPEEDEX instance.
struct SpeedexManagementStructures {
	MemoryDatabase db;
	OrderbookManager orderbook_manager;
	AccountModificationLog account_modification_log;
	BlockHeaderHashMap block_header_hash_map;
	ApproximationParameters approx_params;

	//! Create all of the lmdb instances for Speedex.
	//! Will throw error if databases already exist.
	void create_lmdb() {
		db.create_lmdb();
		orderbook_manager.create_lmdb();
		block_header_hash_map.create_lmdb();
	}

	//! Open all of the lmdb environment instances in Speedex.
	//! LMDB environments are opened before lmdb databases.
	void open_lmdb_env() {
		db.open_lmdb_env();
		orderbook_manager.open_lmdb_env();
		block_header_hash_map.open_lmdb_env();
	}

	//! Open all off the LMDB instances in SPEEDEX
	void open_lmdb() {
		db.open_lmdb();
		orderbook_manager.open_lmdb();
		block_header_hash_map.open_lmdb();
	}

	//! Initialize speedex with a given number of assets and target
	//! approximation bounds
	SpeedexManagementStructures(
		uint16_t num_assets, 
		ApproximationParameters approx_params)
		: db()
		, orderbook_manager(num_assets)
		, account_modification_log()
		, block_header_hash_map()
		, approx_params(approx_params) {}
};


//! All of the data structures involved in running Tatonnement.
struct TatonnementManagementStructures {
	LPSolver lp_solver;
	TatonnementOracle oracle;
	NormalizationRollingAverage rolling_averages;

	//! Init tatonnment objects using supplied speedex objects.
	TatonnementManagementStructures(
		SpeedexManagementStructures& management_structures)
		: lp_solver(management_structures.orderbook_manager)
		, oracle(management_structures.orderbook_manager, lp_solver)
		, rolling_averages(
			management_structures.orderbook_manager.get_num_assets()) {}
};

} /* speedex */