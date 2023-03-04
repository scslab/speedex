#pragma once

/**
 * SPEEDEX: A Scalable, Parallelizable, and Economically Efficient Decentralized Exchange
 * Copyright (C) 2023 Geoffrey Ramseyer

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*! \file serial_transaction_processor.h

Process transactions in a single thread.

Not at all threadsafe.  Use one of these objects
per thread.

Based on template argument, works in block production or validation mode.
(or mocks out various db/orderbooks when loading from lmdb).
*/

#include <cstdint>
#include <memory>

#include "block_processing/operation_metadata.h"

#include "orderbook/orderbook_manager_view.h"

#include "xdr/transaction.h"

#include "utils/debug_macros.h"

namespace speedex {

struct MemoryDatabase;
struct SpeedexManagementStructures;

class BufferedMemoryDatabaseView;
class UnbufferedMemoryDatabaseView;
class LoadLMDBMemoryDatabaseView;

class SerialAccountModificationLog;

/*! Base handler for processing transactions.
Should not be used directly.  Use instead
one of SerialTransactionProcessor (block production)
or SerialTransactionValidator (validation)

All of the process_operations are no-ops if they fail, aside from 
modifications to the account database view (which are undone by unwinding
the db view).
*/

template<typename SerialManager>
class SerialTransactionHandler {

protected:
	using Database = MemoryDatabase;

	SerialManager serial_manager;
	Database& account_database;

	const bool check_sigs;

	//! Create an account
	template<typename DatabaseView>
	TransactionProcessingStatus process_operation(
		OperationMetadata<DatabaseView>& metadata,
		const CreateAccountOp& op);

	//! Create a sell offer
	template<typename DatabaseView>
	TransactionProcessingStatus process_operation(
		OperationMetadata<DatabaseView>& metadata,
		const CreateSellOfferOp& op, 
		SerialAccountModificationLog& serial_account_log);

	//! Cancel a sell offer
	template<typename DatabaseView>
	TransactionProcessingStatus process_operation(
		OperationMetadata<DatabaseView>& metadata,
		const CancelSellOfferOp& op);

	//! Send a payment
	template<typename DatabaseView>
	TransactionProcessingStatus process_operation(
		OperationMetadata<DatabaseView>& metadata,
		const PaymentOp& op);

	//! Money printer go brrr
	template<typename DatabaseView>
	TransactionProcessingStatus process_operation(
		OperationMetadata<DatabaseView>& metadata,
		const MoneyPrinterOp& op);

	//! Utility function for creating an Offer object
	//! from a create sell operation and the transaction metadata.
	template<typename ViewType>
	Offer make_offer(
		const CreateSellOfferOp& op, 
		const OperationMetadata<ViewType>& metadata) {
		Offer offer;
		offer.category = op.category;
		offer.offerId = metadata.operation_id;
		offer.owner = metadata.tx_metadata.sourceAccount;
		offer.amount = op.amount;
		offer.minPrice = op.minPrice;
		return offer;
	}

	//! Log which accounts are modified (and how) by one transaction.
	//! Involves a second iteration over the operations in a transaction.
	//! No-op if the orderbook manager view signals that
	//! we are replaying a trusted block (and thus have no need to rebuild
	//! the modification log).
	//! Should only be called on committed, successful transactions.
	void log_modified_accounts(
		const SignedTransaction& tx, 
		SerialAccountModificationLog& serial_account_log);

public:

	SerialTransactionHandler(
		SpeedexManagementStructures& management_structures, 
		SerialManager&& serial_manager);

	//! Get the underlying manager view.  Used when committing the
	//! serial handler.
	SerialManager& extract_manager_view() {
		return serial_manager;
	}
};

//! Processes transactions in one thread, accumulating new offers locally.
//! For use in block production mode.
class SerialTransactionProcessor 
	: public SerialTransactionHandler<ProcessingSerialManager> {

	using BaseT = SerialTransactionHandler<ProcessingSerialManager>;
	using BaseT::account_database;
	using BaseT::process_operation;
	using Database = typename BaseT::Database;
	using BufferedViewT = BufferedMemoryDatabaseView;
	using UnbufferedViewT = UnbufferedMemoryDatabaseView;
	using BaseT::log_modified_accounts;
	using BaseT::check_sigs;
	using BaseT::serial_manager;

	//! Unwind the creation of a sell offer, when undoing a failed
	//! transaction.
	//! Can only unwind a creation op that succeeded.
	void unwind_operation(
		const OperationMetadata<UnbufferedViewT>& metadata,
		const CreateSellOfferOp& op);

	//! Unwind the cancellation of a sell offer, when undoing a failed
	//! transaction.
	//! Can only unwind a cancellation op that succeeded.
	void unwind_operation(
		const OperationMetadata<UnbufferedViewT>& metadata,
		const CancelSellOfferOp& op);

	//! Unwind the first \a last_valid_op operations in a transaction
	//! (which failed on last_valid_op+1).
	//! Only calls unwind_operation on the ops that succeeded.
	void unwind_transaction(
		const Transaction& tx,
		int32_t last_valid_op);


public:
	//! Initialize new object for processing transactions in one thread.
	SerialTransactionProcessor(
		SpeedexManagementStructures& management_structures);

	//! Process one transaction.  Is no-op if processing fails.
	TransactionProcessingStatus process_transaction(
		const SignedTransaction& tx,
		BlockStateUpdateStatsWrapper& stats,
		SerialAccountModificationLog& serial_account_log);

	using BaseT::extract_manager_view;
};

//! Validate transactions in a single thread.
//! Template argument is for mocking out underlying speedex structures
//! when replaying a trusted block (if said block is already reflected
//! in some lmdbs).
template<typename ManagerViewType = OrderbookManager>
class SerialTransactionValidator 
	: public SerialTransactionHandler<ValidatingSerialManager<ManagerViewType>> {
	using SerialManagerT = ValidatingSerialManager<ManagerViewType>;
	using BaseT = SerialTransactionHandler<SerialManagerT>;
	using BaseT::account_database;
	using BaseT::process_operation;
	using Database = typename BaseT::Database;
	using BaseT::log_modified_accounts;
	using BaseT::serial_manager;
	using BaseT::check_sigs;
	using UnbufferedViewT 	
		= typename std::conditional<
				std::is_same<ManagerViewType, OrderbookManager>::value,
					UnbufferedMemoryDatabaseView,
					LoadLMDBMemoryDatabaseView>
			::type;


	//! Validate one operation.  Returns true on success.
	template<typename OpType, typename... Args>
	bool validate_operation(
		OperationMetadata<UnbufferedViewT>& metadata,
		OpType op,
		Args&... args) {
		auto res = process_operation(metadata, op, args...);
		if (res != TransactionProcessingStatus::SUCCESS)
		{
			TX_INFO("tx failed: result was %lu", res);
		}
		return res == TransactionProcessingStatus::SUCCESS;
	//	return process_operation(metadata, op, args...) 
	//		== TransactionProcessingStatus::SUCCESS;
	}




public:

	//! Initialize one serial tx validator.  Template argument
	//! is empty in typical case, and is round number when replaying
	//! trusted block.
	template<typename ...Args>
	SerialTransactionValidator(
		SpeedexManagementStructures& management_structures, 
		const OrderbookStateCommitmentChecker& orderbook_state_commitment, 
		ThreadsafeValidationStatistics& main_stats,
		Args... lmdb_args);

	//! Validate a single transaction, returns true on success.
	template<typename ...Args>
	bool validate_transaction(
		const SignedTransaction& tx,
		BlockStateUpdateStatsWrapper& stats,
		SerialAccountModificationLog& serial_account_log,
		Args... lmdb_args);

	using BaseT::extract_manager_view;
};

}
