#include "orderbook/commitment_checker.h"

#include "orderbook/utils.h"

#include "mtt/utils/serialize_endian.h"
#include "utils/debug_macros.h"
#include "utils/price.h"

namespace speedex
{

SingleValidationStatistics&
SingleValidationStatistics::operator+=(const SingleValidationStatistics& other)
{
    activated_supply += other.activated_supply;
    return *this;
}

SingleValidationStatistics&
ValidationStatistics::operator[](size_t idx)
{
    return stats[idx];
}

SingleValidationStatistics&
ValidationStatistics::at(size_t idx)
{
    return stats.at(idx);
}

ValidationStatistics&
ValidationStatistics::operator+=(const ValidationStatistics& other)
{
    while (stats.size() < other.stats.size())
    {
        stats.emplace_back();
    }

    for (size_t i = 0; i < other.stats.size(); i++)
    {
        stats.at(i) += other.stats.at(i);
    }
    return *this;
}

void
ValidationStatistics::make_minimum_size(size_t sz)
{
    if (stats.size() <= sz)
    {
        stats.resize(sz + 1);
    }
}

void
ValidationStatistics::log() const
{
    for (std::size_t i = 0; i < stats.size(); i++)
    {
        std::printf("%lf ", stats[i].activated_supply.to_double());
    }
    std::printf("\n");
}

std::size_t
ValidationStatistics::size() const
{
    return stats.size();
}

ThreadsafeValidationStatistics&
ThreadsafeValidationStatistics::operator+=(const ValidationStatistics& other)
{
    std::lock_guard lock(*mtx);
    ValidationStatistics::operator+=(other);
    return *this;
}

void
ThreadsafeValidationStatistics::log()
{
    std::lock_guard lock(*mtx);
    ValidationStatistics::log();
}

FractionalAsset
SingleOrderbookStateCommitmentChecker::fractionalSupplyActivated() const
{
    FractionalAsset::value_t value;
    utils::read_unsigned_big_endian(
        SingleOrderbookStateCommitment::fractionalSupplyActivated, value);
    return FractionalAsset::from_raw(value);
}

FractionalAsset
SingleOrderbookStateCommitmentChecker::partialExecOfferActivationAmount() const
{
    FractionalAsset::value_t value;
    utils::read_unsigned_big_endian(
        SingleOrderbookStateCommitment::partialExecOfferActivationAmount,
        value);
    return FractionalAsset::from_raw(value);
}

bool
SingleOrderbookStateCommitmentChecker::check_threshold_key() const
{
    OfferKeyType zero_key;
    zero_key.fill(0);

    static_assert(
        sizeof(zero_key)
            == sizeof(SingleOrderbookStateCommitment::partialExecThresholdKey),
        "size mismatch");
    // If key is nonzero, "threshold key is null" flag should be 0.
    if (memcmp(zero_key.data(),
               SingleOrderbookStateCommitment::partialExecThresholdKey.data(),
               zero_key.size())
        != 0)
    {

        return SingleOrderbookStateCommitment::thresholdKeyIsNull == 0;
    }

    return SingleOrderbookStateCommitment::thresholdKeyIsNull == 1;
}

void
OrderbookStateCommitmentChecker::log() const
{
    std::printf("fractionalSupplyActivated\n");
    for (std::size_t i = 0; i < commitments.size(); i++)
    {
        std::printf("%lf ",
                    commitments[i].fractionalSupplyActivated().to_double());
    }
    std::printf("\npartialExecOfferActivationAmount\n");

    for (std::size_t i = 0; i < commitments.size(); i++)
    {
        std::printf(
            "%lf ",
            commitments[i].partialExecOfferActivationAmount().to_double());
    }
    std::printf("\n");
}

bool
OrderbookStateCommitmentChecker::check_clearing()
{
    auto num_work_units = commitments.size();
    auto num_assets = prices.size();

    std::vector<FractionalAsset> supplies, demands;
    supplies.resize(num_assets);
    demands.resize(num_assets);

    for (unsigned int i = 0; i < num_work_units; i++)
    {
        auto category = category_from_idx(i, num_assets);

        auto supply_activated = commitments[i].fractionalSupplyActivated();

        supplies[category.sellAsset] += supply_activated;

        auto demanded_raw
            = price::wide_multiply_val_by_a_over_b(supply_activated.value,
                                                   prices[category.sellAsset],
                                                   prices[category.buyAsset]);

        demands[category.buyAsset] += FractionalAsset::from_raw(demanded_raw);
    }

    for (auto i = 0u; i < num_assets; i++)
    {
        FractionalAsset taxed_demand = demands[i].tax(tax_rate);

        CLEARING_INFO("asset %d supplies %lf demands %lf taxed_demand %lf",
                      i,
                      supplies[i].to_double(),
                      demands[i].to_double(),
                      taxed_demand.to_double());
        if (supplies[i] < taxed_demand)
        {
            CLEARING_INFO("invalid clearing: asset %d", i);
            return false;
        }
    }
    return true;
}

bool
OrderbookStateCommitmentChecker::check_stats(
    ThreadsafeValidationStatistics& fully_cleared_stats)
{
    fully_cleared_stats.make_minimum_size(commitments.size());
    for (size_t i = 0; i < commitments.size(); i++)
    {

        if (fully_cleared_stats.at(i).activated_supply
                + commitments.at(i).partialExecOfferActivationAmount()
            != commitments.at(i).fractionalSupplyActivated())
        {
            std::printf(
                "%lu additive mismatch: computed %lf + %lf, expected %lf\n",
                i,
                fully_cleared_stats[i].activated_supply.to_double(),
                commitments[i].partialExecOfferActivationAmount().to_double(),
                commitments[i].fractionalSupplyActivated().to_double());
            return false;
        }
        if (!commitments.at(i).check_threshold_key())
        {
            std::printf("invalid threshold key\n");
            return false;
        }
    }
    return true;
}

} // namespace speedex
