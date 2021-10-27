#include "hotstuff/block_storage/io_utils.h"

#include "utils/debug_macros.h"
#include "utils/hash.h"
#include "utils/save_load_xdr.h"

#include "config.h"

#include <iomanip>
#include <sstream>

namespace hotstuff {

std::string storage_folder = std::string(ROOT_DB_DIRECTORY) + std::string(HOTSTUFF_BLOCKS);

void
make_block_folder() {
	speedex::mkdir_safe(storage_folder.c_str());
}


std::string array_to_str(speedex::Hash const& hash) {
	std::stringstream s;
	s.fill('0');
	for (size_t i = 0; i < hash.size(); i++) {
		s<< std::setw(2) << std::hex << (unsigned short)hash[i];
	}
	return s.str();
}

std::string
block_filename(const HotstuffBlockWire& block)
{
	auto const& header = block.header;

	auto header_hash = speedex::hash_xdr(header);

	return block_filename(header_hash);
}

std::string
block_filename(const speedex::Hash& header_hash)
{
	auto strname = array_to_str(header_hash);

	return storage_folder + strname + std::string(".block");
}

void save_block(const HotstuffBlockWire& block) {
	auto filename = block_filename(block);

	if (speedex::save_xdr_to_file(block, filename.c_str()))
	{
		throw std::runtime_error("failed to save file" + filename);
	}
}

std::optional<HotstuffBlockWire> 
load_block(const speedex::Hash& req_header_hash)
{
	auto filename = block_filename(req_header_hash);

	HotstuffBlockWire block;
	if (speedex::load_xdr_from_file(block, filename.c_str()))
	{
		return std::nullopt;
	}
	return {block};
}

} /* hotstuff */