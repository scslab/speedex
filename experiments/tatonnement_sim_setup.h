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

#include "orderbook/orderbook_manager.h"
#include "orderbook/orderbook_manager_view.h"

#include "xdr/experiments.h"

#include <cstddef>
#include <memory>

namespace speedex {

[[maybe_unused]]
static std::unique_ptr<OrderbookManager>
load_experiment_data(TatonnementExperimentData const& data, size_t num_offers_to_load) {
	auto manager = std::make_unique<OrderbookManager>(data.num_assets);

	ProcessingSerialManager serial_manager(*manager);

	for (size_t i = 0; i < num_offers_to_load; i++) {
		auto& offer = data.offers.at(i);
		auto idx = manager -> look_up_idx(offer.category);
		int unused = 0;
		serial_manager.add_offer(idx, data.offers.at(i), unused, unused);
	}

	serial_manager.finish_merge();

	manager -> commit_for_production(1);
	return manager;
}



} /* speedex */
