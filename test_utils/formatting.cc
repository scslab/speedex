#include "test_utils/formatting.h"

#include "utils/transaction_type_formatter.h"

namespace speedex
{

namespace test
{


Operation
make_payment(AccountID dst, AssetID asset, int64_t amount)
{
	PaymentOp op;
	op.receiver = dst;
	op.asset = asset;
	op.amount = amount;
	return tx_formatter::make_operation(op);
}


}

}