#include "memory_database/sequence_tracker.h"

namespace speedex
{

namespace detail
{

inline uint8_t 
uint64_get_seq_num_offset(uint64_t sequence_number, uint64_t last_committed_id) {

	uint64_t offset = (((sequence_number - last_committed_id) / MAX_OPS_PER_TX) - 1);
	return (offset >= 64 ? 255 : offset);
}

inline uint64_t 
uint64_get_seq_num_increment(uint64_t bv) {
	if (bv == 0) return 0;
	return (64 - __builtin_clzll(bv)) * MAX_OPS_PER_TX;
}

UInt64SequenceTracker::UInt64SequenceTracker(uint64_t last_committed_id)
	: last_committed_id(last_committed_id)
	, sequence_number_vec(0)
{}

UInt64SequenceTracker::UInt64SequenceTracker(UInt64SequenceTracker&& other)
	: last_committed_id(other.last_committed_id)
	, sequence_number_vec(other.sequence_number_vec.load(std::memory_order_acquire))
{}

UInt64SequenceTracker& 
UInt64SequenceTracker::operator=(UInt64SequenceTracker&& other)
{
	last_committed_id = other.last_committed_id;
	sequence_number_vec.store(other.sequence_number_vec.load(std::memory_order_relaxed));
	return *this;
}

TransactionProcessingStatus 
UInt64SequenceTracker::reserve_sequence_number(
		uint64_t sequence_number)
{
	if (sequence_number <= last_committed_id) {
		return TransactionProcessingStatus::SEQ_NUM_TOO_LOW;
	}

	uint8_t offset = uint64_get_seq_num_offset(sequence_number, last_committed_id);

	if (offset >= 64) {
		return TransactionProcessingStatus::SEQ_NUM_TOO_HIGH;
	}

	uint64_t bit_mask = ((uint64_t) 1) << offset;

	uint64_t prev = sequence_number_vec.fetch_or(bit_mask, std::memory_order_relaxed);

	if ((prev & bit_mask) != 0) {
		//some other tx has already reserved the sequence number
		return TransactionProcessingStatus::SEQ_NUM_TEMP_IN_USE;
	}

	return TransactionProcessingStatus::SUCCESS;
}

void
UInt64SequenceTracker::release_sequence_number(uint64_t sequence_number)
{

	if (sequence_number <= last_committed_id) {
		throw std::runtime_error("cannot release invalid seq num!");
	}

	uint8_t offset = uint64_get_seq_num_offset(sequence_number, last_committed_id);

	if (offset >= 64) {
		throw std::runtime_error("cannot release too far forward seq num!");
	}

	uint64_t bit_mask = ~(((uint64_t) 1) << offset);

	sequence_number_vec.fetch_and(bit_mask, std::memory_order_relaxed);
}

void 
UInt64SequenceTracker::commit_sequence_number(
	uint64_t sequence_number)
{}

uint64_t
UInt64SequenceTracker::tentative_commitment() const
{
	return last_committed_id + uint64_get_seq_num_increment(sequence_number_vec.load(std::memory_order_relaxed));
}


void UInt64SequenceTracker::commit()
{
	last_committed_id += uint64_get_seq_num_increment(
	sequence_number_vec.load(std::memory_order_relaxed));
	sequence_number_vec.store(0, std::memory_order_relaxed);
}

void UInt64SequenceTracker::rollback()
{
	sequence_number_vec.store(0, std::memory_order_relaxed);
}
} // detail
} // speedex
