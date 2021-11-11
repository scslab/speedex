/**
 * File derived from libhotstuff/src/crypto.cpp
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

#include "hotstuff/crypto.h"

#include "utils/bitvector.h"

namespace hotstuff {

using speedex::Hash;
using speedex::SecretKey;
using speedex::PublicKey;
using speedex::Signature;
using speedex::ReplicaConfig;
using speedex::ReplicaInfo;
using speedex::ReplicaID;

using xdr::operator==;

bool check_sig(const Signature& sig, const Hash& val, const PublicKey& pk)
{
    return crypto_sign_verify_detached(
        sig.data(), val.data(), val.size(), pk.data()) == 0;
}

PartialCertificate::PartialCertificate(const Hash& _hash, const SecretKey& sk)
    : PartialCertificateWire()
{
    hash = _hash;
    if (crypto_sign_detached(
        sig.data(), //signature
        nullptr, //optional siglen ptr
        hash.data(), //msg
        hash.size(), //msg len
        sk.data())) //sk
    {
        throw std::runtime_error("failed to sign message");
    }
}

PartialCertificate::PartialCertificate(PartialCertificateWire&& wire_cert)
    : PartialCertificateWire(std::move(wire_cert))
    {}

bool 
PartialCertificate::validate(const ReplicaInfo& info) const {
    return check_sig(sig, hash, info.pk);
}

QuorumCertificate::QuorumCertificate(const Hash& obj_hash)
    : obj_hash(obj_hash)
    , sigs() {}

QuorumCertificate::QuorumCertificate(QuorumCertificateWire const& qc_wire)
    : obj_hash(qc_wire.justify)
    , sigs()
{
    speedex::BitVector<ReplicaID> bv(qc_wire.bmp);

    if (bv.size() != qc_wire.sigs.size()) {
        return;
    }

    for (size_t i = 0; i < qc_wire.sigs.size(); i++) {
        ReplicaID id = bv.pop();
        auto const& sig = qc_wire.sigs[i];
        sigs.emplace(id, sig);
    }
}

QuorumCertificateWire 
QuorumCertificate::serialize() const {
    speedex::BitVector<ReplicaID> bv;

    QuorumCertificateWire out;
    out.justify = obj_hash;

    out.sigs.reserve(sigs.size());

    // from lowest to highest rid
    for (auto const& [rid, sig] : sigs)
    {
        out.sigs.push_back(sig);
        bv.add(rid);
    }

    out.bmp = bv.get();

    return out;
}

void 
QuorumCertificate::add_partial_certificate(ReplicaID rid, const PartialCertificate& pc) {
    if (pc.hash != obj_hash) {
        throw std::invalid_argument("partial certificate merged into different quorum certificate");
    }
    if (rid >= speedex::MAX_REPLICAS) {
        throw std::invalid_argument("invalid replica id");
    }
    sigs.emplace(rid, pc.sig);
}

bool QuorumCertificate::verify(const ReplicaConfig &config) const {
    //check for genesis block
    if (obj_hash == speedex::Hash()) {
        return true;
    }

    if (sigs.size() < config.nmajority) return false;
    size_t n_valid = 0;
    for (auto& [rid, sig] : sigs)
    {
        auto const& pk = config.get_publickey(rid);
        if (check_sig(sig, obj_hash, pk)) {
            n_valid ++;
        }
    }
    return n_valid >= config.nmajority;
}

bool QuorumCertificate::has_quorum(const ReplicaConfig& config) const {
    return config.nmajority <= sigs.size();
}

} /* hotstuff */
