#pragma once

#include "xdr/transaction.h"

#include <atomic>
#include <cstdint>

namespace speedex
{

namespace detail
{

class UInt64SequenceTracker
{
	uint64_t last_committed_id;

	std::atomic<uint64_t> sequence_number_vec;

public:

	UInt64SequenceTracker(uint64_t last_committed_id);
	UInt64SequenceTracker(UInt64SequenceTracker&& other);
	UInt64SequenceTracker& operator=(UInt64SequenceTracker&& other);

	void set_last_committed_id(uint64_t id)
	{
		last_committed_id = id;
	}

	uint64_t produce_commitment() const
	{
		return last_committed_id;
	}

	uint64_t tentative_commitment() const;

	//! Reserves a sequence number
	TransactionProcessingStatus reserve_sequence_number(
		uint64_t sequence_number);

	//! Releases a sequence number reservation
	void release_sequence_number(
		uint64_t sequence_number);

	//! Commits a sequence number reservation
	void commit_sequence_number(
		uint64_t sequence_number);

	void commit();
	void rollback();
};

template<uint64_t MAX_SEQ_GAP>
class BoundedSequenceTracker
{
	static_assert(MAX_SEQ_GAP > 64, "small gaps should use UInt64SequenceTracker");

	constexpr static uint64_t NUM_WORDS = (MAX_SEQ_GAP / 64) + 1;

	uint64_t last_committed_id;

	std::array<std::atomic<uint64_t>, NUM_WORDS> sequence_number_vec;

public:

	BoundedSequenceTracker(uint64_t last_committed_id);
	BoundedSequenceTracker(BoundedSequenceTracker&& other);
	BoundedSequenceTracker& operator=(BoundedSequenceTracker&& other);

	void set_last_committed_id(uint64_t id)
	{
		last_committed_id = id;
	}

	uint64_t produce_commitment() const
	{
		return last_committed_id;
	}

	uint64_t tentative_commitment() const;

	//! Reserves a sequence number
	TransactionProcessingStatus reserve_sequence_number(
		uint64_t sequence_number);

	//! Releases a sequence number reservation
	void release_sequence_number(
		uint64_t sequence_number);

	//! Commits a sequence number reservation
	void commit_sequence_number(
		uint64_t sequence_number);

	void commit();
	void rollback();
};

} /* detail */

template<uint64_t MAX_SEQ_GAP>
class SequenceTracker : public std::conditional<MAX_SEQ_GAP <= 64, detail::UInt64SequenceTracker, detail::BoundedSequenceTracker<MAX_SEQ_GAP>>::type
{};



namespace detail
{

inline uint64_t 
array_get_seq_num_offset(uint64_t sequence_number, uint64_t last_committed_id) {

	uint64_t offset = (((sequence_number - last_committed_id) / MAX_OPS_PER_TX) - 1);
	return offset;
}

template<uint64_t NUM_WORDS>
inline uint64_t
array_get_seq_num_increment(std::array<std::atomic<uint64_t>, NUM_WORDS> const& bv)
{
	for (int32_t i = NUM_WORDS - 1; i >= 0; i--)
	{
		uint64_t val = bv[i].load(std::memory_order_relaxed);

		if (val != 0)
		{
			uint64_t out = ((64*i) + (64 - __builtin_clzll(val))) * MAX_OPS_PER_TX;
			return out;
		}
	}
	return 0;
}

template<uint64_t MAX_SEQ_GAP>
BoundedSequenceTracker<MAX_SEQ_GAP>::BoundedSequenceTracker(uint64_t last_committed_id)
	: last_committed_id(last_committed_id)
	, sequence_number_vec()
{}

template<uint64_t MAX_SEQ_GAP>
BoundedSequenceTracker<MAX_SEQ_GAP>::BoundedSequenceTracker(BoundedSequenceTracker&& other)
	: last_committed_id(other.last_committed_id)
	, sequence_number_vec()
{
	for (auto i = 0u; i < NUM_WORDS; i++)
	{
		sequence_number_vec[i].store(other.sequence_number_vec[i].load(std::memory_order_relaxed), std::memory_order_relaxed);
	}
}

template<uint64_t MAX_SEQ_GAP>
BoundedSequenceTracker<MAX_SEQ_GAP>& 
BoundedSequenceTracker<MAX_SEQ_GAP>::operator=(BoundedSequenceTracker&& other)
{
	last_committed_id = other.last_committed_id;

	for (auto i = 0u; i < NUM_WORDS; i++)
	{
		sequence_number_vec[i].store(other.sequence_number_vec[i].load(std::memory_order_relaxed), std::memory_order_relaxed);
	}
	return *this;
}


template<uint64_t MAX_SEQ_GAP>
TransactionProcessingStatus
BoundedSequenceTracker<MAX_SEQ_GAP>::reserve_sequence_number(uint64_t sequence_number)
{
	if (sequence_number <= last_committed_id) {
		return TransactionProcessingStatus::SEQ_NUM_TOO_LOW;
	}

	uint64_t offset = array_get_seq_num_offset(sequence_number, last_committed_id);

	if (offset >= MAX_SEQ_GAP) {
		return TransactionProcessingStatus::SEQ_NUM_TOO_HIGH;
	}

	uint64_t word_offset = offset / 64;
	uint64_t local_offset = offset % 64;

	uint64_t bit_mask = ((uint64_t) 1) << local_offset;

	uint64_t prev = sequence_number_vec[word_offset].fetch_or(bit_mask, std::memory_order_relaxed);

	if ((prev & bit_mask) != 0) {
		//some other tx has already reserved the sequence number
		return TransactionProcessingStatus::SEQ_NUM_TEMP_IN_USE;
	}

	return TransactionProcessingStatus::SUCCESS;
}

template<uint64_t MAX_SEQ_GAP>
void
BoundedSequenceTracker<MAX_SEQ_GAP>::release_sequence_number(
		uint64_t sequence_number)
{
	if (sequence_number <= last_committed_id) {
		throw std::runtime_error("cannot release invalid seq num!");
	}

	uint64_t offset = array_get_seq_num_offset(sequence_number, last_committed_id);

	if (offset >= MAX_SEQ_GAP) {
		throw std::runtime_error("cannot release too far forward seq num!");
	}


	uint64_t word_offset = offset / 64;
	uint64_t local_offset = offset % 64;

	uint64_t bit_mask = ~(((uint64_t) 1) << local_offset);

	sequence_number_vec[word_offset].fetch_and(bit_mask, std::memory_order_relaxed);
}


template<uint64_t MAX_SEQ_GAP>
void
BoundedSequenceTracker<MAX_SEQ_GAP>::commit_sequence_number(
	uint64_t sequence_number)
{}

template<uint64_t MAX_SEQ_GAP>
void
BoundedSequenceTracker<MAX_SEQ_GAP>::commit()
{
	last_committed_id += array_get_seq_num_increment(
		sequence_number_vec);
	
	for (auto& val : sequence_number_vec)
	{
		val.store(0, std::memory_order_relaxed);
	}
}

template<uint64_t MAX_SEQ_GAP>
void
BoundedSequenceTracker<MAX_SEQ_GAP>::rollback()
{
	for (auto& val : sequence_number_vec)
	{
		val.store(0, std::memory_order_relaxed);
	}
}

template<uint64_t MAX_SEQ_GAP>
uint64_t
BoundedSequenceTracker<MAX_SEQ_GAP>::tentative_commitment() const
{
	return last_committed_id + array_get_seq_num_increment(sequence_number_vec);
}

} /* detail */

} /* speedex */
