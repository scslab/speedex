#include "synthetic_data_generator/synthetic_data_gen_options.h"
#include <stdexcept>

namespace speedex {

void CycleDistribution::create_acc_probabilities(
	const double* individual_scores, const int individual_scores_size) 
{
	double sum = 0;
	if (individual_scores_size <= 1) {
		throw std::runtime_error("invalid cycle size distribution");
	}
	for (int i = 0; i < individual_scores_size; i++) {
		//std::printf("sum =%lf individual_scores[i]=%lf\n", sum, individual_scores[i]);
		sum += individual_scores[i];
		acc_probabilities.push_back(sum);
	}
	for (unsigned int i = 0; i < acc_probabilities.size(); i++) {
		acc_probabilities[i] /= sum;
	}
}

int CycleDistribution::get_size(double random) const {
	if (acc_probabilities.size() == 0)
	{
		throw std::runtime_error("can't make a cycle if there are no assets");
	}
	for (unsigned int i = 0; i < acc_probabilities.size(); i++) {
		if (random <= acc_probabilities[i]) {
			return i+2;
		}
	}
	return acc_probabilities.size() + 1;
}

bool 
CycleDistribution::parse_cycle_dist(
	fy_document* fyd, double* score_buf, int* distribution_size, int num_assets) {
	int count = 0;
	count += fy_document_scanf(
		fyd,
		"experiment/cycle_size_dist/dist_max %d",
		distribution_size);

	if (count != 1) {
		return false;
	}

	if (*distribution_size > num_assets) {
		return false;
	}

	char buf[256];

	int score_buf_idx = 0;

	for (int i = 2; i <= *distribution_size; i++) {
		sprintf(buf, "experiment/cycle_size_dist/%u %%u", i);

		uint32_t val;
		count = fy_document_scanf(fyd,
			buf,
			&val);
		if (count != 1) {
			return false;
		}
		if (val == 0)
		{
			return false;
		}
		//std::printf("got idx %lu i %lu val %lf\n", score_buf_idx, i, val);
		score_buf[score_buf_idx] = val;
		score_buf_idx ++;
	}
	return true;

}


bool CycleDistribution::parse(struct fy_document* fyd, int num_assets) {

	double individual_scores[num_assets];
	int distribution_size = 0;
	//idx i stores cumulative probability from 2 to 2+i.  Don't use directly.

	auto status = parse_cycle_dist(fyd, individual_scores, &distribution_size, num_assets);
	if (!status) {
		return status;
	}
	if (distribution_size > 1)
	{
		create_acc_probabilities(individual_scores, distribution_size);
	}
	return true;
}

bool PriceOptions::parse(struct fy_document* fyd) {
	int count = fy_document_scanf(
		fyd,
		"/experiment/prices/price_tolerance_min %lf "		// 1
		"/experiment/prices/price_tolerance_max %lf "		// 2
		"/experiment/prices/price_max %lf "					// 3
		"/experiment/prices/price_min %lf "					// 4
		"/experiment/prices/exp_param %lf "					// 5
		"/experiment/prices/per_block_delta %lf",			// 6
		&min_tolerance,
		&max_tolerance,
		&max_price,
		&min_price,
		&exp_param,
		&per_block_delta);
	return count == 6;
}

bool GenerationOptions::parse(const char* filename) {

	struct fy_document* fyd = fy_document_build_from_file(NULL, filename);
	if (fyd == NULL) {
		std::printf("failed to build doc from file \"%s\"\n", filename);
		return false;
	}
	char filename_buf[256];

	int shuffle;

	int count = fy_document_scanf(
		fyd,
		"/experiment/output_prefix %256s " 					// 1
		"/experiment/num_assets %u "						// 2
		"/experiment/num_accounts %u "						// 3
		"/experiment/account_dist_param %lf "				// 4
		"/experiment/block_size %u "						// 5
		"/experiment/num_blocks %u "						// 6
		"/experiment/bad_tx_fraction %lf "					// 7
		"/experiment/block_boundary_crossing_fraction %lf "	// 8
		"/experiment/initial_endow_min %ld "				// 9
		"/experiment/initial_endow_max %ld "				// 10
		"/experiment/payment_rate %lf "						// 11
		"/experiment/create_offer_rate %lf "				// 12
		"/experiment/account_creation_rate %lf "			// 13
		"/experiment/new_account_balance %ld "				// 14
		"/experiment/cancel_delay_rounds_min %lu "			// 15
		"/experiment/cancel_delay_rounds_max %lu "			// 16
		"/experiment/bad_offer_cancel_chance %lf "			// 17
		"/experiment/good_offer_cancel_chance %lf "			// 18
		"/experiment/do_shuffle %d"							// 19
		,
		filename_buf,
		&num_assets,
		&num_accounts,
		&account_dist_param,
		&block_size,
		&num_blocks,
		&bad_tx_fraction,
		&block_boundary_crossing_fraction,
		&initial_endow_min,
		&initial_endow_max,
		&payment_rate,
		&create_offer_rate,
		&account_creation_rate,
		&new_account_balance,
		&cancel_delay_rounds_min,
		&cancel_delay_rounds_max,
		&bad_offer_cancel_chance,
		&good_offer_cancel_chance,
		&shuffle);

	if (count != 19) {
		std::printf("missing something got %d\n", count);
		return false;
	}

	if (initial_endow_min > initial_endow_max) {
		std::printf("don't be a fool\n");
		return false;
	}
	//std::printf("min endow: %ld, max endow: %ld\n", 
	//	initial_endow_min, initial_endow_max);

	output_prefix = std::string(filename_buf);

	std::printf("output prefix is %s\n", output_prefix.c_str());

	auto status = cycle_dist.parse(fyd, num_assets);

	if (!status) {
		std::printf("cycle problem\n");
		return false;
	}

	status = price_options.parse(fyd);
	if (!status) {
		std::printf("price problem\n");
		return false;
	}

	do_shuffle = (shuffle > 0);

	return true;
}

} /* speedex */