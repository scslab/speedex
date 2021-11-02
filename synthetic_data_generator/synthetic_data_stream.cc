#include "synthetic_data_generator/synthetic_data_stream.h"

#include <xdrpp/marshal.h>

namespace speedex {

std::string 
tx_filename(std::string const& root, uint64_t block_number) {
	return root + std::to_string(block_number) + std::string(".txs");
}

std::pair<uint32_t, std::shared_ptr<xdr::opaque_vec<>>>
SyntheticDataStream::load_txs_unparsed() {
	std::shared_ptr<xdr::opaque_vec<>> data = std::make_shared<xdr::opaque_vec<>>();
	auto filename = tx_filename(folder, cur_block_number);

	if(load_xdr_from_file_fast(*data, filename.c_str(), buffer, BUFFER_SIZE) != 0) {
		return {0, nullptr};
	}

	cur_block_number ++;

	uint32_t num_txs;

	if (data -> size() < 4) {
		return {0, nullptr};
	}

	xdr::xdr_get g(data->data(), data -> data() + 4);

	xdr::xdr_argpack_archive(g, num_txs);

	return {num_txs, data};
}


} /* speedex */
