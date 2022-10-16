#pragma once

#include "xdr/transaction.h"
#include <cstdint>

namespace speedex
{

namespace test
{

[[maybe_unused]]
static uint64_t
make_seqno(uint64_t input)
{
	return input * MAX_OPS_PER_TX;
}

Operation
make_payment(AccountID dst, AssetID asset, int64_t amount);

} // test 

}