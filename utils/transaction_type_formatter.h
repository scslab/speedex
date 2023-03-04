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

#pragma once

#include "xdr/types.h"
#include "xdr/transaction.h"


namespace speedex {

// ensures I don't forget some parameter if I modify types later

namespace tx_formatter {

[[maybe_unused]]
static uint32_t compute_min_fee(const SignedTransaction& tx)
{
	return BASE_FEE_PER_TX + FEE_PER_OP * tx.transaction.operations.size();
}

[[maybe_unused]]
static Operation make_operation(CreateAccountOp op) {
	Operation out;
	out.body.type(CREATE_ACCOUNT);
	out.body.createAccountOp() = op;
	return out;
}

[[maybe_unused]]
static Operation make_operation(CreateSellOfferOp op) {
	Operation out;
	out.body.type(CREATE_SELL_OFFER);
	out.body.createSellOfferOp() = op;
	return out;
}

[[maybe_unused]]
static Operation make_operation(CancelSellOfferOp op) {
	Operation out;
	out.body.type(CANCEL_SELL_OFFER);
	out.body.cancelSellOfferOp() = op;
	return out;
}

[[maybe_unused]]
static Operation make_operation(PaymentOp op) {
	Operation out;
	out.body.type(PAYMENT);
	out.body.paymentOp() = op;
	return out;
}

[[maybe_unused]]
static Operation make_operation(MoneyPrinterOp op) {
	Operation out;
	out.body.type(MONEY_PRINTER);
	out.body.moneyPrinterOp() = op;
	return out;
}

} /* tx_formatter */
} /* speedex */