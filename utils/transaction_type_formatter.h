#pragma once

#include "xdr/types.h"

namespace speedex {

// ensures I don't forget some parameter if I modify types later

namespace tx_formatter {

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