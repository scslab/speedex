#pragma once

#include "synthetic_data_generator/data_stream.h"

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

class SyntheticDataStream : public DataStream {
	constexpr static size_t BUFFER_SIZE = 100'000'000;

	std::string folder;
	uint64_t cur_block_number;

	bool finished;

	unsigned char * const buffer;

public:

	SyntheticDataStream(std::string root_folder)
		: DataStream()
		, folder(root_folder)
		, cur_block_number{1}
		, finished(false)
		, buffer(new unsigned char[BUFFER_SIZE])
		{}

	~SyntheticDataStream()
	{
		delete buffer;
	}

	DataBuffer 
	load_txs_unparsed() override final;

};

} /* speedex */
