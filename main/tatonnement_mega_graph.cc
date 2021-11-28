#include "experiments/tatonnement_sim_experiment.h"

#include "utils/save_load_xdr.h"

#include "xdr/experiments.h"

#include <string>
#include <vector>

using namespace speedex;

int main(int argc, char const *argv[])
{
	if (argc != 3) {
		std::printf("usage: <blah> data_directory outfolder\n");
		return -1;
	}

	std::string experiment_root = std::string(argv[1]) + "/";

	bool small = true;

	std::vector <size_t> num_tx_list = small ?
		std::vector<size_t>({500, 1'000, 5'000, 10'000, 20'000, 50'000, 100'000, 200'000, 500'000}) :
		std::vector<size_t>({500, 1'000, 2'000, 5'000, 10'000, 20'000, 30'000, 40'000, 50'000, 60'000, 70'000, 80'000, 90'000, 100'000, 200'000, 300'000, 400'000, 500'000});

	std::vector<TatonnementExperimentData> trials;

	for (int i = 0; i < 5; i++) {
		std::string filename = experiment_root + std::to_string(i+1) + ".offers";
		trials.emplace_back();
		if (load_xdr_from_file(trials.back(), filename.c_str())) {
			std::printf("filename was %s\n", filename.c_str());
			throw std::runtime_error("failed to load trial data file");
		}
	}

	TatonnementSimExperiment experiment_runner(std::string(argv[2]), trials[0].num_assets);

	std::vector<uint8_t> tax_rates = small? std::vector<uint8_t>({10,15,20}) :
		 std::vector<uint8_t>({5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20});
	std::vector<uint8_t> smooth_mults = small ? std::vector<uint8_t>({5,10,15,20}) :
		 std::vector<uint8_t>({5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20});
	

	for (auto tax_rate : tax_rates) {
		for (auto smooth_mult : smooth_mults) {
			if (experiment_runner.check_preexists(smooth_mult, tax_rate)) {
				continue;
			}
			std::printf("running %u %u\n", smooth_mult, tax_rate);
			experiment_runner.run_experiment(smooth_mult, tax_rate, num_tx_list, trials);
		}
	}

	return 0;
}
