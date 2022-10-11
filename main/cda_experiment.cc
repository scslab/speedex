#include "cda/serial_ob_experiment.h"

#include "synthetic_data_generator/synthetic_data_gen.h"

#include "memory_database/memory_database.h"

#include <utils/time.h>

#include <getopt.h>

using namespace speedex;


[[noreturn]]
static void usage() {
	std::printf(R"(
usage: cda_experiment --exp_options=<experiment_options_yaml, required>
)");
	exit(1);
}
enum opttag {
	EXPERIMENT_OPTIONS,
};

static const struct option opts[] = {
	{"exp_options", required_argument, nullptr, EXPERIMENT_OPTIONS},
	{nullptr, 0, nullptr, 0}
};


int main(int argc, char* const* argv)
{

	size_t num_offers = 500'000;

	std::minstd_rand gen(0);
	GenerationOptions options;

	std::string experiment_options_file;
	
	int opt;

	while ((opt = getopt_long_only(argc, argv, "",
				 opts, nullptr)) != -1)
	{
		switch(opt) {
			case EXPERIMENT_OPTIONS:
				experiment_options_file = optarg;
				break;
			default:
				usage();
		}
	}

	if (experiment_options_file.size() == 0) {
		usage();
	}

	auto parsed = options.parse(experiment_options_file.c_str());
	if (!parsed) {
		std::printf("failed to parse experiment options file\n");
		return 1;
	}

	if (options.num_assets != 2)
	{
		throw std::runtime_error("invalid asset number for cda 2asset experiment");
	}

	MemoryDatabase db;

	GeneratorState generator (gen, options, "foo");

	MemoryDatabaseGenesisData data;
	data.id_list = generator.get_accounts();
	data.pk_list.resize(data.id_list.size());

	auto init_lambda = [&](UserAccount& user)
	{
		user.transfer_available(0, options.new_account_balance);
		user.transfer_available(1, options.new_account_balance);
		user.commit();
	};

	db.install_initial_accounts_and_commit(data, init_lambda);

	std::printf("made init\n");

	size_t num_trials = 5;
	std::vector<std::vector<Offer>> trials;

	for (size_t i = 0; i < num_trials; i++)
	{
		std::printf("making trial i=%d\n", i);
		std::vector<double> prices = generator.gen_prices();
		trials.emplace_back();
		for (size_t j = 0; j < 10; j++)
		{
			auto res = generator.make_offer_list(prices, num_offers/10);
			trials.back().insert(trials.back().end(), res.begin(), res.end());
			generator.modify_prices(prices);
		}
	}

	std::printf("results: db size was %lu\n", db.size());
	for (size_t i = 0; i < num_trials; i++)
	{
		SerialOrderbookExperiment exp(db);
		auto ts = utils::init_time_measurement();

		exp.exec_offers(trials[i]);

		double exec_time = utils::measure_time(ts);

		std::printf("time: %lf tps %lf\n", exec_time, trials[i].size() / exec_time);

	}


}
