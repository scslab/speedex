#pragma once

/*! \file account_modification_log.h

Maintains a log of which accounts were modified while processing a block.
Implicitly assembles a block of transactions during block production.
*/

#include <cstdint>
#include <cinttypes>
#include <thread>

#include "modlog/account_modification_entry.h"
#include "modlog/file_prealloc_worker.h"
#include "modlog/typedefs.h"

#include <mtt/trie/recycling_impl/trie.h>
#include <utils/non_movable.h>

#include "utils/background_deleter.h"
#include <utils/threadlocal_cache.h>

#include "xdr/database_commitments.h"
#include "xdr/types.h"
#include "xdr/block.h"

#include <xdrpp/marshal.h>

namespace speedex {

/*! Log the accounts that are modified when processing a set of transactions.
Given a reference to an AccountModificationLog, a thread gets its threadlocal
copy of SerialAccountModificationLog for doing local modifications.

A reference to this local copy is stored within AccountModificationLog.
When we are done processing a block, AccountModificationLog has a number of
serial logs developed by different threads.  Call batch_merge_in_logs()
to merge these logs into one main log.
*/

struct LogListInsertFn;
struct LogEntryInsertFn;

class AccountModificationLog : private utils::NonMovableOrCopyable {

public:
	
	using LogValueT = 
		AccountModificationEntry;
		//AccountModificationTxListWrapper;

	using TrieT = trie::RecyclingTrie<LogValueT, AccountIDPrefix, TxCountMetadata>;
	using serial_trie_t = TrieT::serial_trie_t;

	static_assert(std::is_same<TrieT::prefix_t, AccountIDPrefix>::value, "unexpected prefix type");

	using serial_cache_t = utils::ThreadlocalCache<serial_trie_t>;

	using saved_block_t = SignedTransactionList; // AccountModificationBlock

private:

	serial_cache_t cache;

	TrieT modification_log;
	std::unique_ptr<saved_block_t> persistable_block;
	mutable std::shared_mutex mtx;
	FilePreallocWorker file_preallocator;
	BackgroundDeleter<saved_block_t> deleter;

	constexpr static unsigned int BUF_SIZE = 5*1677716;
	unsigned char* write_buffer = new unsigned char[BUF_SIZE];
	friend class SerialAccountModificationLog;

public:

	AccountModificationLog() 
		: modification_log()
		, persistable_block(std::make_unique<saved_block_t>())
		, mtx()
		, file_preallocator()
		, deleter() 
		{};

	~AccountModificationLog() {
		std::lock_guard lock(mtx);
		delete[] write_buffer;
	}

	void log_trie() {
		modification_log.log();
	}

	size_t size() const {
		std::shared_lock lock(mtx);
		return modification_log.size();
	}

	//! Apply some function to every value in the log trie.
	template<typename ApplyFn>
	void parallel_iterate_over_log(ApplyFn& fn) const {
		std::shared_lock lock(mtx);
		modification_log.parallel_batch_value_modify(fn);
	}

	//! Merge an accumulated batch of threadlocal trie modifications into
	//! the main trie.
	void merge_in_log_batch();

	//! Hash account modification log.  Also accumulates the block of
	//! transactions from the mod log.
	void hash(Hash& hash);

	//! Clears any leftover resources (sometimes in a background thread).
	void detached_clear();

	//! Prepare a file descriptor for saving account log.
	void prepare_block_fd(uint64_t block_number) {
		file_preallocator.call_prealloc(block_number);
	}

	void cancel_prepare_block_fd() {
		file_preallocator.cancel_prealloc();
	}

	template<typename VectorType>
	void parallel_accumulate_values(VectorType& vec) const {
		std::shared_lock lock(mtx);
		modification_log.template accumulate_values_parallel<VectorType, EntryAccumulateValuesFn>(vec);
	}

	//! Nominally accumulates a vector with all of the values in the trie.
	//! Functions as an interation over all values by keys
	//! a nonstardard operator= into VectorType.
	template<typename VectorType>
	void parallel_accumulate_keys(VectorType& vec) const {
		std::shared_lock lock(mtx);
		modification_log.accumulate_keys_parallel(vec);
	}

	//! Save account block to disk.  Optionally returns the log (for e.g.
	//! forwarding to another node).
	std::unique_ptr<saved_block_t>
	persist_block(uint64_t block_number, bool return_block, bool write_block);

	void diff_with_prev_log(uint64_t block_number);

	// for testing
	void test_metadata_integrity()
	{
		modification_log.test_metadata_integrity_check();
	}
};

/*! For mocks of AccountModificationLog, used when replaying a round
    which we already trust as committed (i.e. in crash recovery). 
*/
class NullModificationLog : public AccountModificationLog {

public:
	using AccountModificationLog::TrieT;
	void merge_in_log( [[maybe_unused]] TrieT&& local_log) {
	}
};

/*! Threadlocal account modification log.

When transaction processing, a thread grabs a serial log for doing its local
modifications.  This log comes from a threadlocal cache held within
the main mod log.  A worker thread modifies this log, and later,
the main log merges this log into itself.
*/
class SerialAccountModificationLog {
	using serial_trie_t = AccountModificationLog::TrieT::serial_trie_t;
	using serial_cache_t = AccountModificationLog::serial_cache_t;

	serial_trie_t& modification_log;
	AccountModificationLog& main_log;

	using LogInsertFn = typename
		std::conditional<std::is_same<AccountModificationLog::LogValueT, AccountModificationTxListWrapper>::value,
			LogListInsertFn,
			LogEntryInsertFn>::type;

public:
	SerialAccountModificationLog(const SerialAccountModificationLog& other) = delete;

	SerialAccountModificationLog(AccountModificationLog& main_log) 
		: modification_log(main_log.cache.get(main_log.modification_log))
		, main_log(main_log) {}

	//! Logs when some operation created by an account results in
	//! a modification to the account, for things other than new txs.
	//! Main example: an offer clears.  Sequence number is of the op
	//! that created the source of the modification (i.e. an offer's id number).
	void 
	log_self_modification(AccountID owner, uint64_t sequence_number);
	
	//! When one account modifies the other, log that the other account
	//! is modified.  Currently happens only when sending payments.
	void 
	log_other_modification(
		AccountID tx_owner, 
		uint64_t sequence_number, 
		AccountID modified_account);
	
	//! Log a new transaction within the trie
	void 
	log_new_self_transaction(const SignedTransaction& tx);

	size_t size() {
		return modification_log.size();
	}
};

} /* speedex */
