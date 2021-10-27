/**
 * File derived from libhotstuff/include/crypto.h
 * Original copyright notice below.
 *
 * Copyright 2018 VMware
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <sodium.h>

#include "config/replica_config.h"

#include "xdr/types.h"
#include "xdr/hotstuff.h"

#include <map>

namespace hotstuff {

struct PartialCertificate : public PartialCertificateWire {

    PartialCertificate(const speedex::Hash& _hash, const speedex::SecretKey& sk);

    PartialCertificate(PartialCertificateWire&& wire_cert);

    bool validate(const speedex::ReplicaInfo& info) const;
};

class QuorumCertificate {

    speedex::Hash obj_hash;
    std::map<speedex::ReplicaID, speedex::Signature> sigs;

public:

    QuorumCertificate(const speedex::Hash& obj_hash);
    QuorumCertificate(QuorumCertificateWire const& qc_wire);

    // for building
    void add_partial_certificate(speedex::ReplicaID rid, const PartialCertificate& pc);
    bool has_quorum(const speedex::ReplicaConfig& config) const; // assumes all certs inserted by add_partial_certificate are valid.

    // for verifying qc from another node
    bool verify(const speedex::ReplicaConfig &config) const;

    const speedex::Hash &get_obj_hash() const {
        return obj_hash;
    }

    QuorumCertificateWire serialize() const;
};

} /* hotstuff */
