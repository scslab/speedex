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
