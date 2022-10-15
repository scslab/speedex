#pragma once

#include <utils/log_collector.h>

namespace speedex
{

class TransferLogs
{
	utils::LogCollector logs;

public:

	void log_transfer(const UserAccount& account, AssetID asset, int64_t amount, const char* reason)
	{
		std::string log
			= std::string("TRANSFER: ") 
			+ std::to_string(account.get_owner()) 
			+ " " 
			+ std::to_string(asset) 
			+ " " 
			+ std::to_string(amount) 
			+ " " 
			+ std::string(reason)
			+ std::string("\n");
		logs.log(log);
	}

	void write_logs(std::string filename)
	{
		if constexpr (LOG_TRANSFERS)
		{
			logs.write_logs(filename);
		}
	}
};

} // speedex
