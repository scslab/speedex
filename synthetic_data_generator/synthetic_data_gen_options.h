#pragma once

#include <vector>
#include <string>

#include <libfyaml.h>

namespace speedex {

class CycleDistribution {
	std::vector<double> acc_probabilities;
	void create_acc_probabilities(
		const double* individual_scores, const int individual_scores_size);
	bool parse_cycle_dist(
		fy_document* fyd, 
		double* score_buf, 
		int* distribution_size, 
		int num_assets);
public:

	int get_size(double random) const; //takes rand in [0,1]
	bool parse(fy_document* fyd, int num_assets);
};

struct PriceOptions {
	double min_price;
	double max_price;
	double min_tolerance;
	double max_tolerance;

	double exp_param;

	double per_block_delta; //intepreted as variance of block price delta

	bool parse(fy_document* fyd);
};

struct GenerationOptions {
	unsigned int num_assets;
	double asset_bias = 0.0;
	unsigned int num_accounts;
	std::string output_prefix;
	double account_dist_param;
	unsigned int block_size;
	unsigned int num_blocks;
	double bad_tx_fraction;
	CycleDistribution cycle_dist;
	PriceOptions price_options;
	double block_boundary_crossing_fraction;

	int64_t initial_endow_min;
	int64_t initial_endow_max;

	double whale_percentage = 0.0;
	int64_t whale_multiplier = 1;
	bool normalize_values = true;

	//payment chance is payment_rate / (payment_rate + create_offer_rate + account_creation_rate), etc.
	double payment_rate;
	double create_offer_rate;
	double account_creation_rate;

	int64_t new_account_balance;

	size_t cancel_delay_rounds_min;
	size_t cancel_delay_rounds_max;
	double bad_offer_cancel_chance; //in [0,1]
	double good_offer_cancel_chance; // in [0,1]

	bool do_shuffle;

	bool reserve_currency = false;


	bool parse(const char* filename);
};

} /* speedex */
