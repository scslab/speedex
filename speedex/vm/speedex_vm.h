#pragma once

#include "block_processing/block_producer.h"
#include "block_processing/block_validator.h"

#include "mempool/mempool_structures.h"

#include "modlog/log_merge_worker.h"

#include "speedex/speedex_management_structures.h"
#include "speedex/speedex_options.h"
#include "speedex/speedex_persistence.h"
//#include "speedex/vm/speedex_vm_block_id.h"

#include "hotstuff/vm/vm_base.h"

#include "xdr/block.h"

#include <mutex>


//namespace hotstuff {
//class HotstuffLMDB;
//} /* hotstuff */

namespace speedex {

class ExperimentParameters;

struct SpeedexVMBlock : public hotstuff::VMBlock
{
	HashedBlockTransactionListPair data;

	SpeedexVMBlock(HashedBlockTransactionListPair const& data)
		: data(data)
		{}

	SpeedexVMBlock(xdr::opaque_vec<> const& vec)
		: data()
		{
			xdr::xdr_from_opaque(vec, data);
		}

	SpeedexVMBlock()
		: data()
		{}

	hotstuff::VMBlockID 
	get_id() const override final
	{
		xdr::opaque_vec<> out = xdr::xdr_to_opaque(data.hashedBlock);
		return hotstuff::VMBlockID(out);
	}

	xdr::opaque_vec<> serialize() const override final
	{
		return xdr::xdr_to_opaque(data);
	}
};


class SpeedexVM : public hotstuff::VMBase {
	const size_t PERSIST_BATCH;
	
	SpeedexManagementStructures management_structures;

	std::mutex operation_mtx;
	std::mutex confirmation_mtx;

	HashedBlock proposal_base_block;
	HashedBlock last_committed_block;
	uint64_t last_persisted_block_number;

	AsyncPersister async_persister;
	
	SpeedexMeasurements measurements_log;

	std::string measurement_output_folder;

	const SpeedexOptions& options;
	const ExperimentParameters& params;

	//constexpr static bool small = false;
	const size_t TARGET_BLOCK_SIZE;
	//constexpr static size_t TARGET_BLOCK_SIZE = small ? 50'000 : 500'000;
	constexpr static size_t MEMPOOL_CHUNK_SIZE = 100;//1'000;
	const size_t MEMPOOL_TARGET_SIZE;

	TatonnementManagementStructures tatonnement_structs;
	std::vector<Price> prices;

	MempoolStructures mempool_structs;

	LogMergeWorker log_merge_worker;

	BlockProducer block_producer;
	BlockValidator block_validator;

	std::atomic<bool> experiment_done = false;

	void rewind_structs_to_committed_height();
	size_t assemble_block(TaggedSingleBlockResults& measurements_base, BlockStateUpdateStatsWrapper& state_update_stats);

	ExperimentResultsUnion 
	get_measurements_nolock();

	std::forward_list<hotstuff::VMBlockID> pending_proposed_ids; 

public:
	using block_type = SpeedexVMBlock;
	using block_id = hotstuff::VMBlockID;
	//using block_type = HashedBlockTransactionListPair;
	//using block_id = SpeedexVMBlockID;

	/*static block_id nonempty_block_id(const block_type& blk) {
		return SpeedexVMBlockID(blk.hashedBlock);
	}
	static block_id empty_block_id() {
		return SpeedexVMBlockID();
	}*/

	SpeedexVM(
		const ExperimentParameters& params,
		const SpeedexOptions& options,
		std::string measurement_output_folder);

	std::unique_ptr<hotstuff::VMBlock>
	try_parse(xdr::opaque_vec<> const& body) override final
	{
		HashedBlockTransactionListPair out;
		try {
			xdr::xdr_from_opaque(body, out);
			return std::make_unique<SpeedexVMBlock>(out);
		} catch(...)
		{
			return nullptr;
		}
	}

	std::unique_ptr<hotstuff::VMBlock> propose() override final;

	void exec_block(const hotstuff::VMBlock&) override final;

	void log_commitment(const block_id& id) override final;

	void write_measurements();
	ExperimentResultsUnion get_measurements();

	std::string 
	overall_measurement_filename() const {
		return measurement_output_folder + "results";
	}

	~SpeedexVM() override final {
		write_measurements();
	}

	void rewind_to_last_commit();

	void init_clean() override final;
	void init_from_disk(hotstuff::LogAccessWrapper const& decided_block_cache) override final;


	// expose state to non-hotstuff components
	bool experiment_is_done() const {
		return experiment_done;
	}

	uint64_t get_lead_block_height();

	Mempool& get_mempool() {
		return mempool_structs.mempool;
	}
};

} /* speedex */
