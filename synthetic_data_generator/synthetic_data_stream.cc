#include "synthetic_data_generator/synthetic_data_stream.h"

namespace speedex {

std::string tx_filename(std::string const& root, uint64_t block_number) {
	return root + std::to_string(block_number) + std::string(".txs");
}

std::shared_ptr<xdr::opaque_vec<>> 
SyntheticDataStream::load_txs() {
	std::shared_ptr<xdr::opaque_vec<>> out = std::make_shared<xdr::opaque_vec<>>();
	auto filename = tx_filename(cur_block_number)

	if(load_xdr_from_file_fast(*data, filename.c_str(), buffer, BUFFER_SIZE) != 0) {
		return nullptr;
	}

	cur_block_number ++;
	return out;
}


} /* speedex */
