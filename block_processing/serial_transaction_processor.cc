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

#include "block_processing/serial_transaction_processor.h"
#include "crypto/crypto_utils.h"

#include "speedex/speedex_management_structures.h"

#include "memory_database/memory_database.h"
#include "memory_database/memory_database_view.h"

#include "modlog/account_modification_log.h"

#include "utils/debug_macros.h"

namespace speedex {

// force template instantiations

template class SerialTransactionValidator<OrderbookManager>;
template class SerialTransactionValidator<LoadLMDBManagerView>;

template
SerialTransactionValidator<OrderbookManager>::SerialTransactionValidator(
	SpeedexManagementStructures& management_structures, 
	const OrderbookStateCommitmentChecker& orderbook_state_commitment, 
	ThreadsafeValidationStatistics& main_stats);

template
SerialTransactionValidator<LoadLMDBManagerView>::SerialTransactionValidator(
	SpeedexManagementStructures& management_structures, 
	const OrderbookStateCommitmentChecker& orderbook_state_commitment, 
	ThreadsafeValidationStatistics& main_stats,
	uint64_t current_block_number); 

template bool SerialTransactionValidator<OrderbookManager>::validate_transaction<>(
	const SignedTransaction&, 
	BlockStateUpdateStatsWrapper& stats, 
	SerialAccountModificationLog& serial_account_log);
template bool SerialTransactionValidator<LoadLMDBManagerView>::validate_transaction<uint64_t>(
	const SignedTransaction&, 
	BlockStateUpdateStatsWrapper& stats, 
	SerialAccountModificationLog& serial_account_log, 
	uint64_t round_number);



inline bool check_tx_format_parameters(const Transaction& tx) {
	if (tx.metadata.sequenceNumber & RESERVED_SEQUENCE_NUM_LOWBITS) {
		return false;
	}
	return true;
}

inline int64_t fee_required(int tx_op_count)
{
	return BASE_FEE_PER_TX + FEE_PER_OP * tx_op_count;
}

inline std::string make_tx_id_string(const TransactionMetadata& metadata) 
{
	return std::string("TX=(")
		+ std::to_string(metadata.sourceAccount)
		+ ", "
		+ std::to_string(metadata.sequenceNumber)
		+ ")";
}


inline bool
is_valid_amount(int64_t amount)
{
	// most that one account can send in a block: max_payment_amount * 256 * 64
	// = max_payment_amount * 2^14 must be at most INT64_MAX = (2^63 - 1),
	// plus fees => 2^15
	// to ensure that there is no trace in a validator that causes an overflow
	constexpr int64_t max_amount = static_cast<int64_t>(1) << (63 - 15);
	return (amount > 0) && (amount <= max_amount);
}



template<typename SerialManager>
SerialTransactionHandler<SerialManager>::SerialTransactionHandler(
	SpeedexManagementStructures& management_structures, 
	SerialManager&& serial_manager)
	: serial_manager(std::move(serial_manager))
	, account_database(management_structures.db)
	, check_sigs(management_structures.configs.check_sigs)
	{}

template<typename SerialManager>
void SerialTransactionHandler<SerialManager>::log_modified_accounts(
	const SignedTransaction& signed_tx, 
	SerialAccountModificationLog& serial_account_log) 
{
	if constexpr (!SerialManager::maintain_account_log) {
		return;
	}
	serial_account_log.log_new_self_transaction(signed_tx);

	auto& tx = signed_tx.transaction;

	int tx_op_count = tx.operations.size();

	for (int i = 0; i < tx_op_count; i++) {
		uint64_t op_seq_number = tx.metadata.sequenceNumber + i;

		auto op_type = tx.operations[i].body.type();

		switch(op_type) {
			case CREATE_ACCOUNT:
				serial_account_log.log_other_modification(
					tx.metadata.sourceAccount, 
					op_seq_number,
					tx.operations[i].body.createAccountOp().newAccountId);
				break;
			case CREATE_SELL_OFFER:
			case CANCEL_SELL_OFFER:
				break; //nothing to do here, we only modify self with these, and we've already logged those.
			case PAYMENT:
				serial_account_log.log_other_modification(
					tx.metadata.sourceAccount,
					op_seq_number,
					tx.operations[i].body.paymentOp().receiver);
				break;
			case MONEY_PRINTER:
				break; // nothing to do here, we only modify self with this, and new txs are already logged.
			default:
				throw std::runtime_error(
					std::string("invalid op type ") + std::to_string(op_type));
		}
	}
}

template<typename ManagerViewType>
template<typename ...Args>
SerialTransactionValidator<ManagerViewType>::SerialTransactionValidator(
	SpeedexManagementStructures& management_structures, 
	const OrderbookStateCommitmentChecker& orderbook_state_commitment, 
	ThreadsafeValidationStatistics& main_stats,
	Args... lmdb_args) 
	: BaseT(
		management_structures, 
		ValidatingSerialManager<ManagerViewType>(
			management_structures.orderbook_manager, 
			orderbook_state_commitment, 
			main_stats,
			lmdb_args...))
{}

template<typename ManagerViewType>
template<typename... Args>
bool SerialTransactionValidator<ManagerViewType>::validate_transaction(
	const SignedTransaction& signed_tx,
	BlockStateUpdateStatsWrapper& stats,
	SerialAccountModificationLog& serial_account_log,
	Args... lmdb_args) {

	auto const& tx = signed_tx.transaction;
	int tx_op_count = tx.operations.size();


	if (!check_tx_format_parameters(tx)) {
		TX_INFO("transaction format parameters failed");
		return false;
	}

	UserAccount* source_account_idx = account_database.lookup_user(tx.metadata.sourceAccount);

	if (source_account_idx == nullptr) {
		TX_INFO("invalid userid lookup %lu", tx.metadata.sourceAccount);
		return false;
	}

	int64_t fee_req = fee_required(tx_op_count);

	if (fee_req > tx.maxFee)
	{
		return false;
	}

	if (check_sigs) {
		if (!sig_check(tx, signed_tx.signature, source_account_idx -> get_pk())) {
			return false;
		}
	}

	OperationMetadata<UnbufferedViewT> op_metadata(
		tx.metadata, source_account_idx, account_database, lmdb_args...);
	op_metadata.set_no_unwind();

	auto id_status = op_metadata.db_view.reserve_sequence_number(
		source_account_idx, tx.metadata.sequenceNumber);

	if (id_status != TransactionProcessingStatus::SUCCESS) {
		TX_INFO("bad seq num on account %lu seqnum %lu", 
			tx.metadata.sourceAccount, tx.metadata.sequenceNumber);
		return false;
	}

	//commit early because no reason not to, maybe marginally better cache perf
	//op_metadata.db_view.commit_sequence_number(
//		source_account_idx, tx.metadata.sequenceNumber);

	if (op_metadata.db_view.transfer_available(
		source_account_idx, MemoryDatabase::NATIVE_ASSET, -fee_req, (make_tx_id_string(tx.metadata) + " fee").c_str())
		!= TransactionProcessingStatus::SUCCESS)
	{
		return false;
	}

	for (int i = 0; i < tx_op_count; i++) {

		op_metadata.operation_id = op_metadata.tx_metadata.sequenceNumber + i;
		auto op_type = tx.operations[i].body.type();

		switch(op_type) {
			case CREATE_ACCOUNT:
				if (!validate_operation(
						op_metadata,
						tx.operations[i].body.createAccountOp())) {
					TX_INFO("create account failed");
					return false;
				}
				break;
			case CREATE_SELL_OFFER:
				if (!validate_operation(
						op_metadata,
						tx.operations[i].body.createSellOfferOp(),
						serial_account_log)) {
					TX_INFO("create sell offer failed");
					return false;
				}
				break;
			case CANCEL_SELL_OFFER:
				if (!validate_operation(
						op_metadata,
						tx.operations[i].body.cancelSellOfferOp())) {
					TX_INFO("cancel sell offer failed");
					return false;
				}
				break;
			case PAYMENT:
				if (!validate_operation(
						op_metadata,
						tx.operations[i].body.paymentOp())) {
					TX_INFO("payment op failed");
					return false;
				}
				break;
			case MONEY_PRINTER:
				if (!validate_operation(
						op_metadata,
						tx.operations[i].body.moneyPrinterOp())) {
					TX_INFO("money printer failed");
					return false;
				}
				break;
			default:
				TX_INFO("garbage operation type");
				return false;
		}
	}
	log_modified_accounts(signed_tx, serial_account_log);
	op_metadata.commit(stats);
	return true;
}

std::string op_type_to_string(OperationType type) {
	switch(type) {
		case CREATE_ACCOUNT:
			return std::string("CREATE_ACCOUNT");
		case CREATE_SELL_OFFER:
			return std::string("CREATE_SELL_OFFER");
		case CANCEL_SELL_OFFER:
			return std::string("CANCEL_SELL_OFFER");
		case PAYMENT:
			return std::string("PAYMENT");
		case MONEY_PRINTER:
			return std::string("MONEY_PRINTER");
		default:
			throw std::runtime_error("invalid operation type");
	}
}

SerialTransactionProcessor::SerialTransactionProcessor(
	SpeedexManagementStructures& management_structures) 
	: BaseT(management_structures, 
		ProcessingSerialManager(management_structures.orderbook_manager)) {}


TransactionProcessingStatus 
SerialTransactionProcessor::process_transaction(
	const SignedTransaction& signed_tx,
	BlockStateUpdateStatsWrapper& stats,
	SerialAccountModificationLog& serial_account_log) {

	auto& tx = signed_tx.transaction;
	
	uint32_t tx_op_count = tx.operations.size();

	TX_INFO("starting process_transaction");

	if (!check_tx_format_parameters(tx)) {

		TX("invalid tx format");
		return TransactionProcessingStatus::INVALID_TX_FORMAT;
	}

	int64_t fee_req = fee_required(tx_op_count);

	if (fee_req > tx.maxFee)
	{
		return TransactionProcessingStatus::FEE_BID_TOO_LOW;
	}

	UserAccount* source_account_idx = account_database.lookup_user(tx.metadata.sourceAccount);
	if (source_account_idx == nullptr) {
		TX_INFO("invalid userid lookup %lu", tx.metadata.sourceAccount);
		return TransactionProcessingStatus::SOURCE_ACCOUNT_NEXIST;
	}

	if (check_sigs) {
		if (!sig_check(tx, signed_tx.signature, source_account_idx -> get_pk())) {
			return TransactionProcessingStatus::BAD_SIGNATURE;
		}
	}

	OperationMetadata<BufferedViewT> op_metadata(
		tx.metadata, source_account_idx, account_database);

	auto seq_num_status = op_metadata.db_view.reserve_sequence_number(
		source_account_idx, tx.metadata.sequenceNumber);

	if (seq_num_status != TransactionProcessingStatus::SUCCESS) {
		TX_INFO("bad seq num on account %lu seqnum %lu", 
			tx.metadata.sourceAccount, tx.metadata.sequenceNumber);
		op_metadata.unwind();
		return seq_num_status;
	}

	TX_INFO("successfully reserved seq num %lu", tx.metadata.sequenceNumber);

	auto fee_status = 
		op_metadata.db_view.transfer_available(
			source_account_idx, MemoryDatabase::NATIVE_ASSET, -fee_req, (make_tx_id_string(tx.metadata) + " fee").c_str());

	if (fee_status != TransactionProcessingStatus::SUCCESS)
	{
		op_metadata.unwind();
		return fee_status;
	}

	for (uint32_t i = 0; i < tx_op_count; i++)
	{
		TX_INFO("processing operation %lu, type %s", 
			i, op_type_to_string(tx.operations[i].body.type()).c_str());

		op_metadata.operation_id = op_metadata.tx_metadata.sequenceNumber + i;

		auto status = TransactionProcessingStatus::INVALID_OPERATION_TYPE;

		switch(tx.operations[i].body.type()) {
			case CREATE_ACCOUNT:
				status = process_operation(
					op_metadata,
					tx.operations[i].body.createAccountOp());
				break;
			case CREATE_SELL_OFFER:
				status = process_operation(
					op_metadata,
					tx.operations[i].body.createSellOfferOp(),
					serial_account_log);
				break;
			case CANCEL_SELL_OFFER:
				status = process_operation(
					op_metadata,
					tx.operations[i].body.cancelSellOfferOp());
				break;
			case PAYMENT:
				status = process_operation(
					op_metadata,
					tx.operations[i].body.paymentOp());
				break;
			case MONEY_PRINTER:
				status = process_operation(
					op_metadata,
					tx.operations[i].body.moneyPrinterOp());
				break;
			// default: status is left as INVALID_OPERATION_TYPE
		}
		if (status != TransactionProcessingStatus::SUCCESS)
		{
			TX_INFO("got bad status from an op");
			unwind_transaction(tx, static_cast<int32_t>(i)-1);
			//op_metadata.db_view.release_sequence_number(
			//	source_account_idx, tx.metadata.sequenceNumber);
			// now unwind handles release sequence number
			op_metadata.unwind();
			return status;
		}
	}

	//op_metadata.db_view.commit_sequence_number(
	//	source_account_idx, tx.metadata.sequenceNumber);
	// now commit handles commit_sequence_number
	op_metadata.commit(stats);

	log_modified_accounts(signed_tx, serial_account_log);
	return TransactionProcessingStatus::SUCCESS;
}

// negative input to last_valid_op makes this a no-op
void 
SerialTransactionProcessor::unwind_transaction(
	const Transaction& tx,
	int32_t last_valid_op) {

	UserAccount* source_account_idx = account_database.lookup_user(tx.metadata.sourceAccount);
	if (source_account_idx == nullptr) {
		throw std::runtime_error("can't unwind tx from nonexistnet acct");
	}

	OperationMetadata<UnbufferedViewT> op_metadata(
		tx.metadata, source_account_idx, account_database);
	op_metadata.set_no_unwind();

	int32_t op_idx = last_valid_op;
	
	while (op_idx >= 0) {

		op_metadata.operation_id 
			= op_metadata.tx_metadata.sequenceNumber + op_idx;


		const Operation& op = tx.operations[op_idx];

		TX("unwinding op of type %d at index %d of %u", 
			op.body.type(), op_idx, tx.operations.size());
		switch(op.body.type()) {
			case CREATE_ACCOUNT:
				// Accounts are unwound by undoing the db view.
				break;
			case CREATE_SELL_OFFER:
				unwind_operation(
					op_metadata,
					op.body.createSellOfferOp());
				break;
			case CANCEL_SELL_OFFER:
				unwind_operation(
					op_metadata,
					op.body.cancelSellOfferOp());
				break;
			case PAYMENT:
				break;
			case MONEY_PRINTER:
				break;
			default:
				throw std::runtime_error("cannot unwind unknown op type");

		}
		op_idx--;
	}
}

//process_operation is a no-op if it returns false.
//validate_operation can do whatever it wants in the case of returning false,
// as this will result in the whole block being reverted later anyways.
//unwind_operation should only be called on ops where process_operation
// returned true.


//CREATE_ACCOUNT

template<typename SerialManager>
template<typename DatabaseView>
TransactionProcessingStatus
SerialTransactionHandler<SerialManager>::process_operation(
	OperationMetadata<DatabaseView>& metadata,
	const CreateAccountOp& op) {

	if (op.startingBalance < CREATE_ACCOUNT_MIN_STARTING_BALANCE) {
		return TransactionProcessingStatus::STARTING_BALANCE_TOO_LOW;
	}

	UserAccount* new_account_idx;
	auto status = metadata.db_view.create_new_account(
		op.newAccountId, op.newAccountPublicKey, &new_account_idx);
	if (status != TransactionProcessingStatus::SUCCESS) {
		return status;
	}
	status = metadata.db_view.transfer_available(
		metadata.source_account_idx,
		Database::NATIVE_ASSET,
		-op.startingBalance,
		(make_tx_id_string(metadata.tx_metadata) + " create account send initial funding").c_str());
	if (status != TransactionProcessingStatus::SUCCESS) {
		return status;
	}
	if (!is_valid_amount(op.startingBalance))
	{
		return TransactionProcessingStatus::INVALID_AMOUNT;
	}

	status = metadata.db_view.transfer_available(
		new_account_idx,
		Database::NATIVE_ASSET,
		op.startingBalance,
		(make_tx_id_string(metadata.tx_metadata) + " create account recv initial funding").c_str());

	if (status != TransactionProcessingStatus::SUCCESS) {
		return status;
	}

	metadata.local_stats.new_account_count ++;
	return TransactionProcessingStatus::SUCCESS;
}

//CREATE_SELL_OFFER

template<typename SerialManager>
template<typename DatabaseView>
TransactionProcessingStatus 
SerialTransactionHandler<SerialManager>::process_operation(
	OperationMetadata<DatabaseView>& metadata,
	const CreateSellOfferOp& op,
	SerialAccountModificationLog& serial_account_log) {

	if (!serial_manager.validate_category(op.category)) {
		TX("invalid category");
		return TransactionProcessingStatus::INVALID_OFFER_CATEGORY;
	}

	if (!price::is_valid_price(op.minPrice)) {
		TX("price out of bounds!");
		return TransactionProcessingStatus::INVALID_PRICE;
	}
	if (!is_valid_amount(op.amount)) {
		TX("amount is 0!");
		return TransactionProcessingStatus::INVALID_AMOUNT;
	}

	int market_idx = serial_manager.look_up_idx(op.category);

	Offer to_add = make_offer(op, metadata);

	// Ignores last two params in block production case,
	// modifies the log when immediately clearing offers in validation case.
	serial_manager.add_offer(market_idx, to_add, metadata, serial_account_log);

	auto status = metadata.db_view.escrow(
		metadata.source_account_idx, op.category.sellAsset, op.amount,
		(make_tx_id_string(metadata.tx_metadata) + " create sell funding").c_str());
	if (status != TransactionProcessingStatus::SUCCESS) {
		TX("escrow failed, unwinding create sell offer: account %lu, account_db_idx %lu, asset %lu, op.amount %lu", 
			metadata.tx_metadata.sourceAccount, metadata.source_account_idx, op.category.sellAsset, op.amount);
		serial_manager.unwind_add_offer(market_idx, to_add);
		return status;
	}

	metadata.local_stats.new_offer_count ++;

	return TransactionProcessingStatus::SUCCESS;
}

void 
SerialTransactionProcessor::unwind_operation(
	const OperationMetadata<UnbufferedViewT>& metadata,
	const CreateSellOfferOp& op) {

	int market_idx = serial_manager.look_up_idx(op.category);
	Offer to_remove = BaseT::make_offer(op, metadata);
	serial_manager.unwind_add_offer(
		market_idx, to_remove);
}

//CANCEL_SELL_OFFER

template<typename SerialManager>
template<typename DatabaseView>
TransactionProcessingStatus 
SerialTransactionHandler<SerialManager>::process_operation(
	OperationMetadata<DatabaseView>& metadata,
	const CancelSellOfferOp& op) {
	int market_idx = serial_manager.look_up_idx(op.category);
	auto found_offer = serial_manager.delete_offer(
		market_idx, 
		op.minPrice, 
		metadata.tx_metadata.sourceAccount, 
		op.offerId);

	if (found_offer) {
		auto status = metadata.db_view.escrow(
			metadata.source_account_idx, 
			op.category.sellAsset, 
			-found_offer -> amount,
			(make_tx_id_string(metadata.tx_metadata) + " cancel offer recv back initial funding").c_str());
		if (status != TransactionProcessingStatus::SUCCESS) {
			serial_manager.undelete_offer(
				market_idx, 
				op.minPrice, 
				metadata.tx_metadata.sourceAccount, 
				op.offerId);
		} else {
			metadata.local_stats.cancel_offer_count++;
		}
		return status;
	}
	//TODO this doesn't distinguish between "the offer isn't present" 
	//and "somebody else already cancelled it"
	return TransactionProcessingStatus::CANCEL_OFFER_TARGET_NEXIST;
}

void 
SerialTransactionProcessor::unwind_operation(
	const OperationMetadata<UnbufferedViewT>& metadata,
	const CancelSellOfferOp& op) {
	int market_idx = serial_manager.look_up_idx(op.category);
	serial_manager.undelete_offer(
		market_idx, 
		op.minPrice, 
		metadata.tx_metadata.sourceAccount, 
		op.offerId);
}

//PAYMENT
template<typename SerialManager>
template<typename DatabaseView>
TransactionProcessingStatus 
SerialTransactionHandler<SerialManager>::process_operation(
	OperationMetadata<DatabaseView>& metadata,
	const PaymentOp& op) {

	UserAccount* target_account_idx = metadata.db_view.lookup_user(op.receiver);

	if (!is_valid_amount(op.amount))
	{
		return TransactionProcessingStatus::INVALID_AMOUNT;
	}
	
	//account_db_idx target_account_idx = -1;
	//if (!metadata.db_view.lookup_user_id(
	//	op.receiver, &target_account_idx)) {
	if (target_account_idx == nullptr) {
		TX("failed to find target account idx");
		return TransactionProcessingStatus::RECIPIENT_ACCOUNT_NEXIST;
	}

	auto status = metadata.db_view.transfer_available(
		metadata.source_account_idx, op.asset, -op.amount,
		(make_tx_id_string(metadata.tx_metadata) + " transfer send").c_str());
	if (status != TransactionProcessingStatus::SUCCESS) {
		return status;
	}
	status = metadata.db_view.transfer_available(
		target_account_idx, op.asset, op.amount,
		(make_tx_id_string(metadata.tx_metadata) + " transfer recv").c_str());
	if (status != TransactionProcessingStatus::SUCCESS) {
		return status;
	}
	metadata.local_stats.payment_count ++;
	return TransactionProcessingStatus::SUCCESS;
}

//MONEY_PRINTER

template<typename SerialManager>
template<typename DatabaseView>
TransactionProcessingStatus
SerialTransactionHandler<SerialManager>::process_operation(
	OperationMetadata<DatabaseView>& metadata,
	const MoneyPrinterOp& op) {

	if (op.amount < 0) {
		return TransactionProcessingStatus::INVALID_PRINT_MONEY_AMOUNT;
	}

	return metadata.db_view.transfer_available(
		metadata.source_account_idx, op.asset, op.amount,
		(make_tx_id_string(metadata.tx_metadata) + " money printer").c_str());
}

} // namespace speedex
