#include <cxxtest/TestSuite.h>

#include "orderbook/orderbook.h"
#include "orderbook/orderbook_manager.h"
#include "orderbook/orderbook_manager_view.h"

#include "speedex/approximation_parameters.h"

#include "utils/debug_macros.h"
#include "utils/price.h"

#include "xdr/types.h"

#include <cstdint>


using namespace speedex;

class DemandCalcTestSuite : public CxxTest::TestSuite {

	void make_basic_orderbook(OrderbookManager& manager) {

		int x = 0;

		OfferCategory category = make_default_category();

		ProcessingSerialManager serial_manager(manager);
		for (int i = 1; i <= 10; i++) {

			Offer offer;
			offer.category = category;
			offer.offerId = i;
			offer.owner = 1;
			offer.amount = 100;
			offer.minPrice = price::from_double(i);

			auto offer_idx = manager.look_up_idx(offer.category);
			serial_manager.add_offer(offer_idx, offer, x, x);
		}
		serial_manager.finish_merge();

		manager.commit_for_production(1);
	}

	OfferCategory make_default_category() {
		OfferCategory category;
		category.sellAsset = 0;
		category.buyAsset = 1;
		category.type = OfferType::SELL;
		return category;
	}

public:

	void test_basic_supply_demand() {
		TEST_START();
		OrderbookManager manager(2);

		ApproximationParameters approx_params{0, 0};

		make_basic_orderbook(manager);

		uint128_t supplies[2];
		uint128_t demands[2];
		Price prices[2];

		for (int i = 0; i < 2; i++) {
			supplies[i] = 0;
			demands[i] = 0;
		}

		prices[1] = 100;
		prices[0] = 500;

		auto unit_idx = manager.look_up_idx(make_default_category());
		auto& orderbooks = manager.get_orderbooks();

		orderbooks[unit_idx].calculate_demands_and_supplies(prices, demands, supplies, approx_params.smooth_mult);

		TS_ASSERT_EQUALS(supplies[0], 500llu << price::PRICE_RADIX);
		TS_ASSERT_EQUALS(demands[1], 2500llu << price::PRICE_RADIX);

		supplies[0] = 0;
		demands[1] = 0;

		prices[0] = 450;

		orderbooks[unit_idx].calculate_demands_and_supplies(prices, demands, supplies, approx_params.smooth_mult);

		TS_ASSERT_EQUALS(supplies[0], 400llu << price::PRICE_RADIX);
		TS_ASSERT_EQUALS(demands[1], 1800llu << price::PRICE_RADIX);

		supplies[0] = 0;
		demands[1] = 0;

		prices[0] = 80;
		
		orderbooks[unit_idx].calculate_demands_and_supplies(prices, demands, supplies, approx_params.smooth_mult);

		TS_ASSERT_EQUALS(supplies[0], 0);
		TS_ASSERT_EQUALS(demands[1], 0);

		supplies[0] = 0;
		demands[1] = 0;

		prices[0] = 1200;
		
		orderbooks[unit_idx].calculate_demands_and_supplies(prices, demands, supplies, approx_params.smooth_mult);

		TS_ASSERT_EQUALS(supplies[0], 1000llu << price::PRICE_RADIX);
		TS_ASSERT_EQUALS(demands[1], 12000llu << price::PRICE_RADIX);
	}

	void test_basic_sd_times_price() {
			TEST_START();
		OrderbookManager manager(2);

		ApproximationParameters approx_params{0, 0};

		make_basic_orderbook(manager);

		uint128_t supplies[2];
		uint128_t demands[2];
		Price prices[2];

		for (int i = 0; i < 2; i++) {
			supplies[i] = 0;
			demands[i] = 0;
		}

		prices[1] = 100;
		prices[0] = 500;

		auto unit_idx = manager.look_up_idx(make_default_category());
		auto& orderbooks = manager.get_orderbooks();

		orderbooks[unit_idx].calculate_demands_and_supplies_times_prices(prices, demands, supplies, approx_params.smooth_mult);

		TS_ASSERT_EQUALS(supplies[0], 250000llu);
		TS_ASSERT_EQUALS(demands[1], 250000llu);

		supplies[0] = 0;
		demands[1] = 0;

		prices[0] = 450;

		orderbooks[unit_idx].calculate_demands_and_supplies_times_prices(prices, demands, supplies, approx_params.smooth_mult);

		TS_ASSERT_EQUALS(supplies[0], 180000llu);
		TS_ASSERT_EQUALS(demands[1], 180000llu);

		supplies[0] = 0;
		demands[1] = 0;

		prices[0] = 80;
		
		orderbooks[unit_idx].calculate_demands_and_supplies_times_prices(prices, demands, supplies, approx_params.smooth_mult);

		TS_ASSERT_EQUALS(supplies[0], 0);
		TS_ASSERT_EQUALS(demands[1], 0);

		supplies[0] = 0;
		demands[1] = 0;

		prices[0] = 1200;
		
		orderbooks[unit_idx].calculate_demands_and_supplies_times_prices(prices, demands, supplies, approx_params.smooth_mult);

		TS_ASSERT_EQUALS(supplies[0], 1200000llu);
		TS_ASSERT_EQUALS(demands[1], 1200000llu);
	}

	void test_smooth_mult() {
		TEST_START();
		OrderbookManager manager(2);
		
		ApproximationParameters approx_params{0, 2};
		
		make_basic_orderbook(manager);

		uint128_t supplies[2];
		uint128_t demands[2];
		Price prices[2];

		for (int i = 0; i < 2; i++) {
			supplies[i] = 0;
			demands[i] = 0;
		}

		prices[1] = 100;
		prices[0] = 800;

		auto unit_idx = manager.look_up_idx(make_default_category());
		auto& orderbooks = manager.get_orderbooks();

		orderbooks[unit_idx].calculate_demands_and_supplies(prices, demands, supplies, approx_params.smooth_mult);

		TS_ASSERT_EQUALS(supplies[0], 650llu << price::PRICE_RADIX);
		TS_ASSERT_EQUALS(demands[1], 5200llu << price::PRICE_RADIX);
	}

	void test_smooth_mult_times_price() {
		TEST_START();
		OrderbookManager manager(2);
		
		ApproximationParameters approx_params{0, 2};
		
		make_basic_orderbook(manager);

		uint128_t supplies[2];
		uint128_t demands[2];
		Price prices[2];

		for (int i = 0; i < 2; i++) {
			supplies[i] = 0;
			demands[i] = 0;
		}

		prices[1] = 100;
		prices[0] = 800;

		auto unit_idx = manager.look_up_idx(make_default_category());
		auto& orderbooks = manager.get_orderbooks();

		orderbooks[unit_idx].calculate_demands_and_supplies_times_prices(prices, demands, supplies, approx_params.smooth_mult);

		TS_ASSERT_EQUALS(supplies[0], 520000llu);
		TS_ASSERT_EQUALS(demands[1], 520000llu);
	}


	void test_max_feasible_smooth_mult() {
		TEST_START();

		OrderbookManager manager(2);

		make_basic_orderbook(manager);
		
		Price prices[2];

		prices[1] = 100;
		prices[0] = 800;

		auto& orderbooks = manager.get_orderbooks();

		TS_ASSERT_EQUALS(orderbooks[0].max_feasible_smooth_mult(800, prices), 255);
		TS_ASSERT_EQUALS(orderbooks[0].max_feasible_smooth_mult(701, prices), 255);
		TS_ASSERT_EQUALS(orderbooks[0].max_feasible_smooth_mult(700, prices), 255);
		TS_ASSERT_EQUALS(orderbooks[0].max_feasible_smooth_mult(699, prices), 3);

	}

};