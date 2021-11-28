#pragma once

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
