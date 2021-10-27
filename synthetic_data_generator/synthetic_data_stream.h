#pragma once

#include "utils/save_load_xdr.h"

#include <cstdint>
#include <memory>

#include <xdrpp/types.h>

namespace speedex {

/*
//TODO future: rotating system of buffers to avoid excessive copying.
class BufferManager {

	using buffer_t = std::shared_ptr<std::vector<unsigned char>>;

};*/

class SyntheticDataStream {
	constexpr static size_t BUFFER_SIZE = 100'000'000;

	std::string folder;
	uint64_t cur_block_number;

	unsigned char * const buffer;

public:

	SyntheticDataStream(std::string root_folder)
		: folder(root_folder)
		, cur_block_number{1}
		, buffer(new unsigned char[BUFFER_SIZE])
		{}

	~SyntheticDataStream()
	{
		delete buffer;
	}

	std::shared_ptr<xdr::opaque_vec<>> load_txs_unparsed();

};

} /* speedex */
