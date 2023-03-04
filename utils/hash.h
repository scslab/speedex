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

/*! \file hash.h
Hash xdr objects
*/

#include <sodium.h>
#include <xdrpp/marshal.h>

#include "xdr/types.h"

namespace speedex {

//! Hash an xdr (serializable) type.
//! Must call sodium_init() prior to usage.
template<typename xdr_type>
Hash hash_xdr(const xdr_type& value) {
	Hash hash_buf;
	auto serialized = xdr::xdr_to_opaque(value);

	if (crypto_generichash(
		hash_buf.data(), hash_buf.size(), 
		serialized.data(), serialized.size(), 
		NULL, 0) != 0) 
	{
		throw std::runtime_error("error in crypto_generichash");
	}
	return hash_buf;
}

} /* speedex */