/*
 *
 *    Copyright (c) 2026 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */
#include <device/types/root-node/features/CodeDrivenTlsDelegates.h>

#include <app/clusters/tls-certificate-management-server/CertificateTableImpl.h>
#include <app/clusters/tls-certificate-management-server/IncrementingIdHelper.h>
#include <app/storage/FabricTableImpl.ipp>
#include <clusters/TlsCertificateManagement/Commands.h>
#include <clusters/TlsClientManagement/Commands.h>
#include <crypto/CHIPCryptoPAL.h>
#include <lib/support/CHIPMem.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/DefaultStorageKeyAllocator.h>
#include <lib/support/PersistentData.h>
#include <lib/support/logging/CHIPLogging.h>

#include <array>
#include <new>

using namespace chip;
using namespace chip::app;
using namespace chip::app::DataModel;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::Tls;
using namespace chip::app::Clusters::TlsCertificateManagement;
using namespace chip::app::Clusters::TlsCertificateManagement::Structs;
using namespace chip::app::Storage;
using namespace chip::app::Storage::Data;
using namespace chip::Crypto;
using namespace chip::Platform;
using namespace Protocols::InteractionModel;

namespace {

constexpr uint16_t kSpecMaxCertBytes     = 3000;
constexpr uint16_t kMaxIntermediateCerts = 10;

struct InlineBufferedRootCert : CertificateTable::BufferedRootCert
{
    PersistenceBuffer<CHIP_CONFIG_TLS_PERSISTED_ROOT_CERT_BYTES> buffer;
    InlineBufferedRootCert() : CertificateTable::BufferedRootCert(buffer) {}
};

struct InlineBufferedClientCert : CertificateTable::BufferedClientCert
{
    PersistenceBuffer<CHIP_CONFIG_TLS_PERSISTED_CLIENT_CERT_BYTES> buffer;
    InlineBufferedClientCert() : CertificateTable::BufferedClientCert(buffer) {}
};

struct RefEncodableRootCert
{
    explicit RefEncodableRootCert(TLSCertStruct::Type & cert) : certificate(&cert) {}
    RefEncodableRootCert() = default;

    TLSCertStruct::Type * certificate = nullptr;
    std::array<uint8_t, kSpecMaxCertBytes> certBytes{};

    CHIP_ERROR FromPersistence(FabricIndex fabric, const CertificateTable::RootCertStruct & src)
    {
        VerifyOrReturnError(certificate != nullptr, CHIP_ERROR_INTERNAL);

        MutableByteSpan targetBytes(certBytes);
        ReturnErrorOnFailure(CopySpanToMutableSpan(src.certificate.Value(), targetBytes));

        certificate->fabricIndex = fabric;
        certificate->caid        = src.caid;
        certificate->certificate.SetValue(targetBytes);
        return CHIP_NO_ERROR;
    }
};

struct InlineEncodableRootCert : RefEncodableRootCert
{
    TLSCertStruct::Type inlineCertificate;

    InlineEncodableRootCert() : RefEncodableRootCert(inlineCertificate) {}
};

struct RefEncodableClientCert
{
    explicit RefEncodableClientCert(TLSClientCertificateDetailStruct::Type & cert) : certificate(&cert) {}
    RefEncodableClientCert() = default;

    TLSClientCertificateDetailStruct::Type * certificate = nullptr;
    std::array<uint8_t, kSpecMaxCertBytes> certBytes{};
    std::array<ByteSpan, kMaxIntermediateCerts> intermediateCerts{};
    std::array<std::array<uint8_t, kSpecMaxCertBytes>, kMaxIntermediateCerts> intermediateCertBytes{};

    CHIP_ERROR FromPersistence(FabricIndex fabric, const CertificateTable::ClientCertStruct & src)
    {
        VerifyOrReturnError(certificate != nullptr, CHIP_ERROR_INTERNAL);

        if (!src.clientCertificate.Value().IsNull())
        {
            MutableByteSpan targetBytes(certBytes);
            ReturnErrorOnFailure(CopySpanToMutableSpan(src.clientCertificate.Value().Value(), targetBytes));
            certificate->clientCertificate.SetValue(Nullable<ByteSpan>(targetBytes));
        }
        else
        {
            certificate->clientCertificate.SetValue(Nullable<ByteSpan>());
        }

        if (src.intermediateCertificates.HasValue())
        {
            auto srcIter = src.intermediateCertificates.Value().begin();
            uint8_t i    = 0;
            while (srcIter.Next())
            {
                VerifyOrReturnError(i < kMaxIntermediateCerts, CHIP_ERROR_NO_MEMORY);
                MutableByteSpan targetIntermediateBytes(intermediateCertBytes[i]);
                ReturnErrorOnFailure(CopySpanToMutableSpan(srcIter.GetValue(), targetIntermediateBytes));
                intermediateCerts[i++] = targetIntermediateBytes;
            }
            ReturnErrorOnFailure(srcIter.GetStatus());
            certificate->intermediateCertificates.SetValue(List<ByteSpan>(intermediateCerts.data(), i));
        }
        else
        {
            certificate->intermediateCertificates.SetValue(List<ByteSpan>());
        }

        certificate->fabricIndex = fabric;
        certificate->ccdid       = src.ccdid;
        return CHIP_NO_ERROR;
    }
};

struct InlineEncodableClientCert : RefEncodableClientCert
{
    TLSClientCertificateDetailStruct::Type inlineCertificate;

    InlineEncodableClientCert() : RefEncodableClientCert(inlineCertificate) {}
};

CHIP_ERROR FingerprintMatch(const ByteSpan & fingerprint, const ByteSpan & cert, bool & outMatch)
{
    std::array<uint8_t, kSHA256_Hash_Length> fingerprintPayload{};
    MutableByteSpan calculatedFingerprint(fingerprintPayload);
    ReturnErrorOnFailure(Hash_SHA256(cert.data(), cert.size(), fingerprintPayload.data()));
    outMatch = fingerprint.data_equal(calculatedFingerprint);
    return CHIP_NO_ERROR;
}

enum class TagEndpoint : uint8_t
{
    kTlsEndpointId,
    kEndpointPayload,
    kNextId,
    kStoredFabricIndex,
    kEndpointMapping,
};

constexpr size_t kPersistentBufferNextIdBytes = TLV::EstimateStructOverhead(
    sizeof(uint16_t), // mNextId
    TLV::EstimateStructOverhead(sizeof(EndpointId), sizeof(FabricIndex)) *
        (static_cast<size_t>(kMaxCodeDrivenTlsEndpoints) * CHIP_CONFIG_MAX_FABRICS)); // mEndpointMapping

constexpr size_t kTlsEndpointMaxBytes =
    TLV::EstimateStructOverhead(sizeof(FabricIndex),                                            // Fabric ID
                                sizeof(uint16_t),                                               // endpointID
                                TLV::EstimateStructOverhead(sizeof(uint16_t),                   // endpointID
                                                            sizeof(uint8_t) * kSpecMaxHostname, // hostname
                                                            sizeof(uint16_t),                   // port
                                                            sizeof(uint16_t),                   // caid
                                                            sizeof(Nullable<uint16_t>),         // ccdid
                                                            sizeof(uint8_t),                    // referenceCount
                                                            sizeof(FabricIndex)));              // fabricIndex

struct BufferedEndpoint
{
    TLSClientManagementDelegate::EndpointStructType mEndpoint;
    PersistenceBuffer<kTlsEndpointMaxBytes> mBuffer;
};

class GlobalEndpointData : public PersistableData<kPersistentBufferNextIdBytes>
{
    IncrementingIdHelper<CodeDrivenTlsEndpointId, static_cast<size_t>(kMaxCodeDrivenTlsEndpoints) * CHIP_CONFIG_MAX_FABRICS>
        mEndpoints;
    EndpointId mEndpointId = kInvalidEndpointId;

public:
    explicit GlobalEndpointData(EndpointId endpoint) : mEndpointId(endpoint) {}
    ~GlobalEndpointData() override = default;

    void Clear() override { mEndpoints.Clear(); }

    CHIP_ERROR UpdateKey(StorageKeyName & key) const override
    {
        VerifyOrReturnError(kInvalidEndpointId != mEndpointId, CHIP_ERROR_INVALID_ARGUMENT);
        key = DefaultStorageKeyAllocator::TlsClientEndpointGlobalDataKey(mEndpointId);
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR Serialize(TLV::TLVWriter & writer) const override
    {
        TLV::TLVType container;
        ReturnErrorOnFailure(writer.StartContainer(TLV::AnonymousTag(), TLV::kTLVType_Structure, container));
        ReturnErrorOnFailure(writer.Put(TLV::ContextTag(TagEndpoint::kNextId), mEndpoints.mNextId));

        TLV::TLVType entryMapContainer;
        ReturnErrorOnFailure(
            writer.StartContainer(TLV::ContextTag(TagEndpoint::kEndpointMapping), TLV::kTLVType_Array, entryMapContainer));
        ReturnErrorOnFailure(mEndpoints.SerializeMapping(writer, TLV::ContextTag(TagEndpoint::kTlsEndpointId),
                                                         TLV::ContextTag(TagEndpoint::kStoredFabricIndex)));
        ReturnErrorOnFailure(writer.EndContainer(entryMapContainer));

        return writer.EndContainer(container);
    }

    CHIP_ERROR Deserialize(TLV::TLVReader & reader) override
    {
        ReturnErrorOnFailure(reader.Next(TLV::kTLVType_Structure, TLV::AnonymousTag()));

        TLV::TLVType container;
        ReturnErrorOnFailure(reader.EnterContainer(container));
        ReturnErrorOnFailure(reader.Next(TLV::ContextTag(TagEndpoint::kNextId)));
        ReturnErrorOnFailure(reader.Get(mEndpoints.mNextId));

        TLV::TLVType entryMapContainer;
        ReturnErrorOnFailure(reader.Next(TLV::kTLVType_Array, TLV::ContextTag(TagEndpoint::kEndpointMapping)));
        ReturnErrorOnFailure(reader.EnterContainer(entryMapContainer));
        ReturnErrorOnFailure(mEndpoints.DeserializeMapping(reader, TLV::ContextTag(TagEndpoint::kTlsEndpointId),
                                                           TLV::ContextTag(TagEndpoint::kStoredFabricIndex)));
        ReturnErrorOnFailure(reader.ExitContainer(entryMapContainer));

        return reader.ExitContainer(container);
    }

    CHIP_ERROR Load(PersistentStorageDelegate * storage) // NOLINT(bugprone-derived-method-shadowing-base-method)
    {
        CHIP_ERROR err = PersistableData::Load(storage);
        return err.NoErrorIf(CHIP_ERROR_NOT_FOUND); // NOT_FOUND is OK; DataAccessor::Load already called Clear()
    }

    CHIP_ERROR GetNextId(FabricIndex fabric, uint16_t & id)
    {
        ReturnErrorOnFailure(mEndpoints.GetNextId(UINT16_MAX));
        return mEndpoints.ConsumeId(fabric, id);
    }

    CHIP_ERROR Remove(PersistentStorageDelegate & storage, CodeDrivenTlsEndpointTable & table, FabricIndex fabric, uint16_t id)
    {
        return mEndpoints.Remove(table, fabric, CodeDrivenTlsEndpointId(id), [&, this]() { return Save(&storage); });
    }

    CHIP_ERROR RemoveAll(PersistentStorageDelegate & storage, FabricIndex fabric)
    {
        return mEndpoints.RemoveAll(fabric, [&, this]() { return Save(&storage); });
    }
};

using InnerIterator = TableEntryDataConvertingIterator<CodeDrivenTlsEndpointId, TLSClientManagementDelegate::EndpointStructType>;

} // namespace

namespace chip::app::Storage {

using EndpointSerializer = DefaultSerializer<CodeDrivenTlsEndpointId, TLSClientManagementDelegate::EndpointStructType>;

template <>
StorageKeyName DefaultSerializer<CodeDrivenTlsEndpointId, TLSClientManagementDelegate::EndpointStructType>::EndpointEntryCountKey(
    EndpointId endpoint_id)
{
    return DefaultStorageKeyAllocator::TlsClientEndpointCountKey(endpoint_id);
}

template <>
StorageKeyName
DefaultSerializer<CodeDrivenTlsEndpointId, TLSClientManagementDelegate::EndpointStructType>::FabricEntryDataKey(FabricIndex fabric,
                                                                                                                EndpointId endpoint)
{
    return DefaultStorageKeyAllocator::TlsClientEndpointFabricDataKey(fabric, endpoint);
}

template <>
StorageKeyName DefaultSerializer<CodeDrivenTlsEndpointId, TLSClientManagementDelegate::EndpointStructType>::FabricEntryKey(
    FabricIndex fabric, EndpointId endpoint, uint16_t idx)
{
    return DefaultStorageKeyAllocator::TlsClientEndpointEntityKey(fabric, endpoint, idx);
}

template <>
constexpr size_t DefaultSerializer<CodeDrivenTlsEndpointId, TLSClientManagementDelegate::EndpointStructType>::kEntryMaxBytes()
{
    return kTlsEndpointMaxBytes;
}

template <>
constexpr uint16_t DefaultSerializer<CodeDrivenTlsEndpointId, TLSClientManagementDelegate::EndpointStructType>::kMaxPerFabric()
{
    return kMaxCodeDrivenTlsEndpoints;
}

template <>
constexpr uint16_t DefaultSerializer<CodeDrivenTlsEndpointId, TLSClientManagementDelegate::EndpointStructType>::kMaxPerEndpoint()
{
    return UINT16_MAX;
}

template <>
CHIP_ERROR DefaultSerializer<CodeDrivenTlsEndpointId, TLSClientManagementDelegate::EndpointStructType>::SerializeId(
    TLV::TLVWriter & writer, const CodeDrivenTlsEndpointId & id)
{
    return writer.Put(TLV::ContextTag(TagEndpoint::kTlsEndpointId), id.mEndpointId);
}

template <>
CHIP_ERROR DefaultSerializer<CodeDrivenTlsEndpointId, TLSClientManagementDelegate::EndpointStructType>::DeserializeId(
    TLV::TLVReader & reader, CodeDrivenTlsEndpointId & id)
{
    ReturnErrorOnFailure(reader.Next(TLV::ContextTag(TagEndpoint::kTlsEndpointId)));
    return reader.Get(id.mEndpointId);
}

template <>
CHIP_ERROR DefaultSerializer<CodeDrivenTlsEndpointId, TLSClientManagementDelegate::EndpointStructType>::SerializeData(
    TLV::TLVWriter & writer, const TLSClientManagementDelegate::EndpointStructType & data)
{
    return data.EncodeForRead(writer, TLV::ContextTag(TagEndpoint::kEndpointPayload), data.GetFabricIndex());
}

template <>
CHIP_ERROR DefaultSerializer<CodeDrivenTlsEndpointId, TLSClientManagementDelegate::EndpointStructType>::DeserializeData(
    TLV::TLVReader & reader, TLSClientManagementDelegate::EndpointStructType & data)
{
    ReturnErrorOnFailure(reader.Next(TLV::kTLVType_Structure, TLV::ContextTag(TagEndpoint::kEndpointPayload)));
    return data.Decode(reader);
}

template <>
void DefaultSerializer<CodeDrivenTlsEndpointId, TLSClientManagementDelegate::EndpointStructType>::Clear(
    TLSClientManagementDelegate::EndpointStructType & data)
{
    new (&data) TLSClientManagementDelegate::EndpointStructType();
}

} // namespace chip::app::Storage

template class chip::app::Storage::FabricTableImpl<CodeDrivenTlsEndpointId, TLSClientManagementDelegate::EndpointStructType>;

namespace chip {
namespace app {

Status CodeDrivenTlsCertificateManagementDelegate::ProvisionRootCert(EndpointId matterEndpoint, FabricIndex fabric,
                                                                     const ProvisionRootCertificateType & provisionReq,
                                                                     Tls::TLSCAID & outCaid)
{
    VerifyOrReturnValue(matterEndpoint == mEndpointId, Status::ConstraintError);

    auto localId = provisionReq.caid.IsNull() ? Optional<Tls::TLSCAID>() : Optional<Tls::TLSCAID>(provisionReq.caid.Value());
    UniquePtr<InlineBufferedRootCert> certBuffer(New<InlineBufferedRootCert>());
    VerifyOrReturnValue(certBuffer, Status::ResourceExhausted);

    auto result = mCertificateTable.UpsertRootCertificateEntry(fabric, localId, certBuffer->buffer, provisionReq.certificate);

    VerifyOrReturnValue(result == CHIP_NO_ERROR, Status::Failure);
    outCaid = localId.Value();
    return Status::Success;
}

CHIP_ERROR CodeDrivenTlsCertificateManagementDelegate::LoadedRootCerts(EndpointId matterEndpoint, FabricIndex fabric,
                                                                       LoadedRootCertificateCallback loadedCallback) const
{
    VerifyOrReturnError(matterEndpoint == mEndpointId, CHIP_IM_GLOBAL_STATUS(ConstraintError));

    UniquePtr<InlineBufferedRootCert> certBuffer(New<InlineBufferedRootCert>());
    UniquePtr<InlineEncodableRootCert> callbackCert(New<InlineEncodableRootCert>());
    VerifyOrReturnError(certBuffer && callbackCert, CHIP_ERROR_NO_MEMORY);

    return mCertificateTable.IterateRootCertificates(fabric, *certBuffer, [&](auto & iterator) -> CHIP_ERROR {
        while (iterator.Next(certBuffer->GetCert()))
        {
            ReturnErrorOnFailure(callbackCert->FromPersistence(fabric, certBuffer->GetCert()));
            ReturnErrorOnFailure(loadedCallback(callbackCert->inlineCertificate));
        }
        return CHIP_NO_ERROR;
    });
}

CHIP_ERROR CodeDrivenTlsCertificateManagementDelegate::RootCertsForFabric(EndpointId matterEndpoint, FabricIndex fabric,
                                                                          RootCertificateListCallback loadedCallback) const
{
    VerifyOrReturnError(matterEndpoint == mEndpointId, CHIP_IM_GLOBAL_STATUS(ConstraintError));

    uint8_t numRootCerts = 0;
    ReturnErrorOnFailure(mCertificateTable.GetRootCertificateCount(fabric, numRootCerts));
    ScopedMemoryBuffer<InlineEncodableRootCert> rootCertificatePayloads;
    ScopedMemoryBuffer<RootCertStructType> rootCertificates;
    rootCertificatePayloads.Alloc(numRootCerts);
    rootCertificates.Alloc(numRootCerts);
    UniquePtr<InlineBufferedRootCert> certBuffer(New<InlineBufferedRootCert>());
    VerifyOrReturnError(certBuffer && !rootCertificatePayloads.IsNull() && !rootCertificates.IsNull(), CHIP_ERROR_NO_MEMORY);

    uint8_t i   = 0;
    auto result = mCertificateTable.IterateRootCertificates(fabric, *certBuffer, [&](auto & iterator) -> CHIP_ERROR {
        while (iterator.Next(certBuffer->GetCert()))
        {
            rootCertificatePayloads[i].certificate = &rootCertificates[i];
            ReturnErrorOnFailure(rootCertificatePayloads[i++].FromPersistence(fabric, certBuffer->GetCert()));
        }
        return CHIP_NO_ERROR;
    });
    ReturnErrorOnFailure(result);

    List<const RootCertStructType> rootCertificatesList(rootCertificates.Get(), numRootCerts);
    return loadedCallback(rootCertificatesList);
}

CHIP_ERROR CodeDrivenTlsCertificateManagementDelegate::FindRootCert(EndpointId matterEndpoint, FabricIndex fabric, Tls::TLSCAID id,
                                                                    LoadedRootCertificateCallback loadedCallback) const
{
    VerifyOrReturnError(matterEndpoint == mEndpointId, CHIP_IM_GLOBAL_STATUS(ConstraintError));

    UniquePtr<InlineEncodableRootCert> callbackCert(New<InlineEncodableRootCert>());
    UniquePtr<InlineBufferedRootCert> certBuffer(New<InlineBufferedRootCert>());
    VerifyOrReturnError(callbackCert && certBuffer, CHIP_ERROR_NO_MEMORY);

    ReturnErrorOnFailure(mCertificateTable.GetRootCertificateEntry(fabric, id, *certBuffer));
    ReturnErrorOnFailure(callbackCert->FromPersistence(fabric, certBuffer->GetCert()));
    return loadedCallback(callbackCert->inlineCertificate);
}

CHIP_ERROR CodeDrivenTlsCertificateManagementDelegate::LookupRootCertByFingerprint(
    EndpointId matterEndpoint, FabricIndex fabric, const ByteSpan & fingerprint, LoadedRootCertificateCallback loadedCallback) const
{
    VerifyOrReturnError(matterEndpoint == mEndpointId, CHIP_IM_GLOBAL_STATUS(ConstraintError));

    UniquePtr<InlineBufferedRootCert> certBuffer(New<InlineBufferedRootCert>());
    VerifyOrReturnError(certBuffer, CHIP_ERROR_NO_MEMORY);
    return mCertificateTable.IterateRootCertificates(fabric, *certBuffer, [&](auto & iterator) -> CHIP_ERROR {
        while (iterator.Next(certBuffer->GetCert()))
        {
            bool match = false;
            ReturnErrorOnFailure(FingerprintMatch(fingerprint, certBuffer->GetCert().certificate.Value(), match));
            if (match)
            {
                UniquePtr<InlineEncodableRootCert> callbackCert(New<InlineEncodableRootCert>());
                VerifyOrReturnError(callbackCert, CHIP_ERROR_NO_MEMORY);

                ReturnErrorOnFailure(callbackCert->FromPersistence(fabric, certBuffer->GetCert()));
                return loadedCallback(callbackCert->inlineCertificate);
            }
        }
        return CHIP_ERROR_NOT_FOUND;
    });
}

CHIP_ERROR CodeDrivenTlsCertificateManagementDelegate::LookupRootCert(EndpointId matterEndpoint, FabricIndex fabric,
                                                                      const ByteSpan & certificate,
                                                                      LoadedRootCertificateCallback loadedCallback) const
{
    std::array<uint8_t, kSHA256_Hash_Length> fingerprintPayload{};
    MutableByteSpan calculatedFingerprint(fingerprintPayload);
    ReturnErrorOnFailure(Hash_SHA256(certificate.data(), certificate.size(), fingerprintPayload.data()));
    return LookupRootCertByFingerprint(matterEndpoint, fabric, calculatedFingerprint, loadedCallback);
}

Status CodeDrivenTlsCertificateManagementDelegate::RemoveRootCert(EndpointId matterEndpoint, FabricIndex fabric, Tls::TLSCAID id)
{
    VerifyOrReturnValue(matterEndpoint == mEndpointId, Status::ConstraintError);

    auto result = mCertificateTable.RemoveRootCertificate(fabric, id);
    if (result == CHIP_ERROR_NOT_FOUND)
    {
        return Status::NotFound;
    }

    VerifyOrReturnValue(result == CHIP_NO_ERROR, Status::Failure);
    return Status::Success;
}

Status CodeDrivenTlsCertificateManagementDelegate::GenerateClientCsr(EndpointId matterEndpoint, FabricIndex fabric,
                                                                     const ClientCsrType & request,
                                                                     GeneratedCsrCallback loadedCallback) const
{
    VerifyOrReturnValue(matterEndpoint == mEndpointId, Status::ConstraintError);

    ScopedMemoryBuffer<uint8_t> csrData;
    csrData.Alloc(kSpecMaxCertBytes);
    VerifyOrReturnValue(csrData, Status::ResourceExhausted);
    MutableByteSpan csr(csrData.Get(), kSpecMaxCertBytes);

    std::array<uint8_t, 128> nonceData{};
    MutableByteSpan nonceSignature(nonceData);

    UniquePtr<InlineBufferedClientCert> certBuffer(New<InlineBufferedClientCert>());
    VerifyOrReturnValue(certBuffer, Status::ResourceExhausted);

    Optional<TLSCCDID> id;
    if (!request.ccdid.IsNull())
    {
        id.SetValue(request.ccdid.Value());
    }
    auto result = mCertificateTable.PrepareClientCertificate(fabric, request.nonce, certBuffer->buffer, id, csr, nonceSignature);
    ReturnValueOnFailure(result, Status::Failure);

    ClientCsrResponseType csrResponse;
    csrResponse.ccdid          = id.Value();
    csrResponse.csr            = csr;
    csrResponse.nonceSignature = nonceSignature;
    return loadedCallback(csrResponse);
}

Status CodeDrivenTlsCertificateManagementDelegate::ProvisionClientCert(EndpointId matterEndpoint, FabricIndex fabric,
                                                                       const ProvisionClientCertificateType & provisionReq)
{
    VerifyOrReturnValue(matterEndpoint == mEndpointId, Status::ConstraintError);

    UniquePtr<InlineBufferedClientCert> certBuffer(New<InlineBufferedClientCert>());
    VerifyOrReturnValue(certBuffer, Status::ResourceExhausted);

    TLSClientCertificateDetailStruct::DecodableType details;
    details.ccdid = provisionReq.ccdid;
    details.clientCertificate.SetValue(provisionReq.clientCertificate);
    details.intermediateCertificates.SetValue(provisionReq.intermediateCertificates);
    details.SetFabricIndex(fabric);

    auto result = mCertificateTable.UpdateClientCertificateEntry(fabric, provisionReq.ccdid, certBuffer->buffer, details);
    if (result == CHIP_ERROR_INVALID_ARGUMENT)
    {
        return Status::DynamicConstraintError;
    }
    ReturnValueOnFailure(result, Status::Failure);
    return Status::Success;
}

CHIP_ERROR CodeDrivenTlsCertificateManagementDelegate::LoadedClientCerts(EndpointId matterEndpoint, FabricIndex fabric,
                                                                         LoadedClientCertificateCallback loadedCallback) const
{
    VerifyOrReturnError(matterEndpoint == mEndpointId, CHIP_IM_GLOBAL_STATUS(ConstraintError));

    UniquePtr<InlineBufferedClientCert> certBuffer(New<InlineBufferedClientCert>());
    UniquePtr<InlineEncodableClientCert> callbackCert(New<InlineEncodableClientCert>());
    VerifyOrReturnError(certBuffer && callbackCert, CHIP_ERROR_NO_MEMORY);

    return mCertificateTable.IterateClientCertificates(fabric, *certBuffer, [&](auto & iterator) -> CHIP_ERROR {
        while (iterator.Next(certBuffer->mCertWithKey))
        {
            ReturnErrorOnFailure(callbackCert->FromPersistence(fabric, certBuffer->GetCert()));
            ReturnErrorOnFailure(loadedCallback(callbackCert->inlineCertificate));
        }
        return CHIP_NO_ERROR;
    });
}

CHIP_ERROR CodeDrivenTlsCertificateManagementDelegate::ClientCertsForFabric(EndpointId matterEndpoint, FabricIndex fabric,
                                                                            ClientCertificateListCallback loadedCallback) const
{
    VerifyOrReturnError(matterEndpoint == mEndpointId, CHIP_IM_GLOBAL_STATUS(ConstraintError));

    uint8_t numClientCerts = 0;
    ReturnErrorOnFailure(mCertificateTable.GetClientCertificateCount(fabric, numClientCerts));
    UniquePtr<InlineBufferedClientCert> certBuffer(New<InlineBufferedClientCert>());
    ScopedMemoryBuffer<RefEncodableClientCert> clientCertificatePayloads;
    ScopedMemoryBuffer<ClientCertStructType> clientCertificates;
    clientCertificatePayloads.Alloc(numClientCerts);
    clientCertificates.Alloc(numClientCerts);
    VerifyOrReturnError(certBuffer && clientCertificatePayloads && clientCertificates, CHIP_ERROR_NO_MEMORY);

    uint8_t i   = 0;
    auto result = mCertificateTable.IterateClientCertificates(fabric, *certBuffer, [&](auto & iterator) -> CHIP_ERROR {
        while (iterator.Next(certBuffer->mCertWithKey))
        {
            clientCertificatePayloads[i].certificate = &clientCertificates[i];
            ReturnErrorOnFailure(clientCertificatePayloads[i++].FromPersistence(fabric, certBuffer->GetCert()));
        }
        return CHIP_NO_ERROR;
    });
    ReturnErrorOnFailure(result);

    List<const ClientCertStructType> clientCertificatesList(clientCertificates.Get(), numClientCerts);
    return loadedCallback(clientCertificatesList);
}

CHIP_ERROR CodeDrivenTlsCertificateManagementDelegate::FindClientCert(EndpointId matterEndpoint, FabricIndex fabric, TLSCCDID id,
                                                                      LoadedClientCertificateCallback loadedCallback) const
{
    VerifyOrReturnError(matterEndpoint == mEndpointId, CHIP_IM_GLOBAL_STATUS(ConstraintError));

    UniquePtr<InlineBufferedClientCert> certBuffer(New<InlineBufferedClientCert>());
    UniquePtr<InlineEncodableClientCert> callbackCert(New<InlineEncodableClientCert>());
    VerifyOrReturnError(certBuffer && callbackCert, CHIP_ERROR_NO_MEMORY);

    ReturnErrorOnFailure(mCertificateTable.GetClientCertificateEntry(fabric, id, *certBuffer));
    ReturnErrorOnFailure(callbackCert->FromPersistence(fabric, certBuffer->GetCert()));
    return loadedCallback(callbackCert->inlineCertificate);
}

CHIP_ERROR
CodeDrivenTlsCertificateManagementDelegate::LookupClientCertByFingerprint(EndpointId matterEndpoint, FabricIndex fabric,
                                                                          const ByteSpan & fingerprint,
                                                                          LoadedClientCertificateCallback loadedCallback) const
{
    VerifyOrReturnError(matterEndpoint == mEndpointId, CHIP_IM_GLOBAL_STATUS(ConstraintError));

    UniquePtr<InlineBufferedClientCert> certBuffer(New<InlineBufferedClientCert>());
    VerifyOrReturnError(certBuffer, CHIP_ERROR_NO_MEMORY);

    return mCertificateTable.IterateClientCertificates(fabric, *certBuffer, [&](auto & iterator) -> CHIP_ERROR {
        while (iterator.Next(certBuffer->mCertWithKey))
        {
            const auto & cert = certBuffer->GetCert();
            if (cert.clientCertificate.Value().IsNull())
            {
                continue;
            }
            bool match = false;
            ReturnErrorOnFailure(FingerprintMatch(fingerprint, cert.clientCertificate.Value().Value(), match));
            if (match)
            {
                UniquePtr<InlineEncodableClientCert> callbackCert(New<InlineEncodableClientCert>());
                VerifyOrReturnError(callbackCert, CHIP_ERROR_NO_MEMORY);

                ReturnErrorOnFailure(callbackCert->FromPersistence(fabric, cert));
                return loadedCallback(callbackCert->inlineCertificate);
            }
        }
        return CHIP_ERROR_NOT_FOUND;
    });
}

CHIP_ERROR CodeDrivenTlsCertificateManagementDelegate::LookupClientCert(EndpointId matterEndpoint, FabricIndex fabric,
                                                                        const ByteSpan & certificate,
                                                                        LoadedClientCertificateCallback loadedCallback) const
{
    std::array<uint8_t, kSHA256_Hash_Length> fingerprintPayload{};
    MutableByteSpan calculatedFingerprint(fingerprintPayload);
    ReturnErrorOnFailure(Hash_SHA256(certificate.data(), certificate.size(), fingerprintPayload.data()));
    return LookupClientCertByFingerprint(matterEndpoint, fabric, calculatedFingerprint, loadedCallback);
}

Status CodeDrivenTlsCertificateManagementDelegate::RemoveClientCert(EndpointId matterEndpoint, FabricIndex fabric, Tls::TLSCCDID id)
{
    VerifyOrReturnValue(matterEndpoint == mEndpointId, Status::ConstraintError);

    auto result = mCertificateTable.RemoveClientCertificate(fabric, id);
    if (result == CHIP_ERROR_NOT_FOUND)
    {
        return Status::NotFound;
    }

    VerifyOrReturnValue(result == CHIP_NO_ERROR, Status::Failure);
    return Status::Success;
}

CHIP_ERROR CodeDrivenTlsClientManagementDelegate::Init(PersistentStorageDelegate & storage)
{
    mStorage = &storage;
    mProvisioned.SetEndpoint(mEndpointId);
    return mProvisioned.Init(storage);
}

CHIP_ERROR CodeDrivenTlsClientManagementDelegate::ForEachEndpoint(EndpointId matterEndpoint, FabricIndex fabric,
                                                                  LoadedEndpointCallback callback)
{
    VerifyOrReturnError(matterEndpoint == mEndpointId, CHIP_ERROR_INTERNAL);

    UniquePtr<BufferedEndpoint> endpoint(New<BufferedEndpoint>());
    VerifyOrReturnError(endpoint, CHIP_ERROR_NO_MEMORY);
    return mProvisioned.IterateEntries(fabric, endpoint->mBuffer, [&](auto & iterator) {
        InnerIterator innerIter(iterator);
        while (innerIter.Next(endpoint->mEndpoint))
        {
            ReturnErrorOnFailure(callback(endpoint->mEndpoint));
        }
        return CHIP_NO_ERROR;
    });
}

CHIP_ERROR CodeDrivenTlsClientManagementDelegate::GetEndpointId(FabricIndex fabric, uint16_t & id)
{
    VerifyOrReturnError(mStorage != nullptr, CHIP_ERROR_INCORRECT_STATE);

    UniquePtr<GlobalEndpointData> globalData(New<GlobalEndpointData>(mEndpointId));
    VerifyOrReturnError(globalData, CHIP_ERROR_NO_MEMORY);
    ReturnErrorOnFailure(globalData->Load(mStorage));
    ReturnErrorOnFailure(globalData->GetNextId(fabric, id));

    return globalData->Save(mStorage);
}

ClusterStatusCode CodeDrivenTlsClientManagementDelegate::ProvisionEndpoint(
    EndpointId matterEndpoint, FabricIndex fabric,
    const TlsClientManagement::Commands::ProvisionEndpoint::DecodableType & provisionReq, uint16_t & endpointID)
{
    VerifyOrReturnValue(matterEndpoint == mEndpointId, ClusterStatusCode(Status::ConstraintError));
    VerifyOrReturnValue(mStorage != nullptr, ClusterStatusCode(Status::ConstraintError));

    uint16_t numInFabric = 0;

    ClusterStatusCode installedCheck(Status::Failure);
    UniquePtr<BufferedEndpoint> endpoint(New<BufferedEndpoint>());
    VerifyOrReturnValue(endpoint, ClusterStatusCode(Status::ResourceExhausted));
    CHIP_ERROR entryCheck = mProvisioned.IterateEntries(fabric, endpoint->mBuffer, [&](auto & iterator) {
        InnerIterator innerIter(iterator);
        while (innerIter.Next(endpoint->mEndpoint))
        {
            numInFabric++;
            auto & endpointStruct = endpoint->mEndpoint;
            if (endpointStruct.hostname.data_equal(provisionReq.hostname) && (endpointStruct.port == provisionReq.port) &&
                (provisionReq.endpointID.IsNull() || provisionReq.endpointID.Value() != endpointStruct.endpointID))
            {
                installedCheck =
                    ClusterStatusCode::ClusterSpecificFailure(TlsClientManagement::StatusCodeEnum::kEndpointAlreadyInstalled);
                return CHIP_ERROR_BAD_REQUEST;
            }
        }
        return CHIP_NO_ERROR;
    });
    VerifyOrReturnValue(entryCheck == CHIP_NO_ERROR, installedCheck);

    if (provisionReq.endpointID.IsNull())
    {
        VerifyOrReturnValue(mTLSClientManagementCluster != nullptr, ClusterStatusCode(Status::Failure));
        VerifyOrReturnValue(numInFabric < mTLSClientManagementCluster->GetMaxProvisioned(),
                            ClusterStatusCode(Status::ResourceExhausted));
        EndpointSerializer::Clear(endpoint->mEndpoint);

        uint16_t nextId = 0;
        ReturnValueOnFailure(GetEndpointId(fabric, nextId), ClusterStatusCode(Status::ResourceExhausted));
        endpoint->mEndpoint.endpointID = nextId;
        endpointID                     = endpoint->mEndpoint.endpointID;
    }
    else
    {
        CodeDrivenTlsEndpointId localId(provisionReq.endpointID.Value());
        ReturnValueOnFailure(mProvisioned.GetTableEntry(fabric, localId, endpoint->mEndpoint, endpoint->mBuffer),
                             ClusterStatusCode(Status::NotFound));
        endpointID = provisionReq.endpointID.Value();
    }

    auto & endpointStruct         = endpoint->mEndpoint;
    endpointStruct.hostname       = provisionReq.hostname;
    endpointStruct.port           = provisionReq.port;
    endpointStruct.caid           = provisionReq.caid;
    endpointStruct.ccdid          = provisionReq.ccdid;
    endpointStruct.referenceCount = 0;
    endpointStruct.SetFabricIndex(fabric);

    CodeDrivenTlsEndpointId localId(endpointID);
    ReturnValueOnFailure(mProvisioned.SetTableEntry(fabric, localId, endpointStruct, endpoint->mBuffer),
                         ClusterStatusCode(Status::Failure));

    return ClusterStatusCode(Status::Success);
}

CHIP_ERROR CodeDrivenTlsClientManagementDelegate::FindProvisionedEndpointByID(EndpointId matterEndpoint, FabricIndex fabric,
                                                                              uint16_t endpointID, LoadedEndpointCallback callback)
{
    VerifyOrReturnError(matterEndpoint == mEndpointId, CHIP_ERROR_INTERNAL);

    CodeDrivenTlsEndpointId localId(endpointID);
    UniquePtr<BufferedEndpoint> endpoint(New<BufferedEndpoint>());
    VerifyOrReturnError(endpoint, CHIP_ERROR_NO_MEMORY);
    ReturnErrorOnFailure(mProvisioned.GetTableEntry(fabric, localId, endpoint->mEndpoint, endpoint->mBuffer));
    return callback(endpoint->mEndpoint);
}

Status CodeDrivenTlsClientManagementDelegate::RemoveProvisionedEndpointByID(EndpointId matterEndpoint, FabricIndex fabric,
                                                                            uint16_t endpointID)
{
    VerifyOrReturnValue(matterEndpoint == mEndpointId, Status::ConstraintError);
    VerifyOrReturnValue(mStorage != nullptr, Status::ConstraintError);

    UniquePtr<BufferedEndpoint> endpoint(New<BufferedEndpoint>());
    VerifyOrReturnValue(endpoint, Status::ResourceExhausted);
    CodeDrivenTlsEndpointId localId(endpointID);
    CHIP_ERROR result = mProvisioned.GetTableEntry(fabric, localId, endpoint->mEndpoint, endpoint->mBuffer);
    VerifyOrReturnValue(result != CHIP_ERROR_NOT_FOUND, Status::NotFound);
    VerifyOrReturnValue(result == CHIP_NO_ERROR, Status::Failure);
    VerifyOrReturnValue(endpoint->mEndpoint.referenceCount == 0, Status::InvalidInState);

    UniquePtr<GlobalEndpointData> globalData(New<GlobalEndpointData>(matterEndpoint));
    VerifyOrReturnValue(globalData, Status::ResourceExhausted);
    ReturnValueOnFailure(globalData->Load(mStorage), Status::Failure);
    result = globalData->Remove(*mStorage, mProvisioned, fabric, endpointID);
    VerifyOrReturnValue(result != CHIP_ERROR_NOT_FOUND, Status::NotFound);
    VerifyOrReturnValue(result == CHIP_NO_ERROR, Status::Failure);

    return Status::Success;
}

CHIP_ERROR CodeDrivenTlsClientManagementDelegate::RootCertCanBeRemoved(EndpointId matterEndpoint, FabricIndex fabric,
                                                                       Tls::TLSCAID id)
{
    VerifyOrReturnError(matterEndpoint == mEndpointId, CHIP_ERROR_INTERNAL);

    UniquePtr<BufferedEndpoint> endpoint(New<BufferedEndpoint>());
    VerifyOrReturnError(endpoint, CHIP_ERROR_NO_MEMORY);
    return mProvisioned.IterateEntries(fabric, endpoint->mBuffer, [&](auto & iterator) {
        InnerIterator innerIter(iterator);
        while (innerIter.Next(endpoint->mEndpoint))
        {
            VerifyOrReturnError(endpoint->mEndpoint.caid != id, CHIP_ERROR_BAD_REQUEST);
        }
        return CHIP_NO_ERROR;
    });
}

CHIP_ERROR CodeDrivenTlsClientManagementDelegate::ClientCertCanBeRemoved(EndpointId matterEndpoint, FabricIndex fabric,
                                                                         Tls::TLSCCDID id)
{
    VerifyOrReturnError(matterEndpoint == mEndpointId, CHIP_ERROR_INTERNAL);

    UniquePtr<BufferedEndpoint> endpoint(New<BufferedEndpoint>());
    VerifyOrReturnError(endpoint, CHIP_ERROR_NO_MEMORY);
    return mProvisioned.IterateEntries(fabric, endpoint->mBuffer, [&](auto & iterator) {
        InnerIterator innerIter(iterator);
        while (innerIter.Next(endpoint->mEndpoint))
        {
            VerifyOrReturnError(endpoint->mEndpoint.ccdid != id, CHIP_ERROR_BAD_REQUEST);
        }
        return CHIP_NO_ERROR;
    });
}

void CodeDrivenTlsClientManagementDelegate::RemoveFabric(FabricIndex fabric)
{
    VerifyOrReturn(mStorage != nullptr);
    VerifyOrReturn(mProvider != nullptr, ChipLogError(AppServer, "No data model provider on fabric removal."));

    ReturnAndLogOnFailure(mProvisioned.RemoveFabric(*mProvider, fabric), AppServer, "Failure clearing TLS endpoints for fabric");

    UniquePtr<GlobalEndpointData> globalData(New<GlobalEndpointData>(mEndpointId));
    VerifyOrReturn(globalData);
    ReturnOnFailure(globalData->Load(mStorage));

    // Not having endpoint data for a fabric is not an error; the fabric may never
    // have had TLS endpoints provisioned.
    ReturnAndLogOnFailure(globalData->RemoveAll(*mStorage, fabric).NoErrorIf(CHIP_ERROR_NOT_FOUND), AppServer,
                          "Failure clearing TLS endpoint data for fabric");
}

CHIP_ERROR CodeDrivenTlsClientManagementDelegate::MutateEndpointReferenceCount(EndpointId matterEndpoint, FabricIndex fabric,
                                                                               uint16_t endpointID, int8_t delta)
{
    VerifyOrReturnError(matterEndpoint == mEndpointId, CHIP_ERROR_INTERNAL);
    VerifyOrReturnError(mStorage != nullptr, CHIP_ERROR_INTERNAL);

    UniquePtr<BufferedEndpoint> endpoint(New<BufferedEndpoint>());
    VerifyOrReturnError(endpoint, CHIP_ERROR_NO_MEMORY);
    CodeDrivenTlsEndpointId localId(endpointID);
    ReturnErrorOnFailure(mProvisioned.GetTableEntry(fabric, localId, endpoint->mEndpoint, endpoint->mBuffer));

    if ((0 - delta) <= endpoint->mEndpoint.referenceCount)
    {
        VerifyOrReturnError((static_cast<int16_t>(delta) + static_cast<int16_t>(endpoint->mEndpoint.referenceCount)) <= UINT8_MAX,
                            CHIP_ERROR_INVALID_ARGUMENT);
        endpoint->mEndpoint.referenceCount = static_cast<uint8_t>(endpoint->mEndpoint.referenceCount + delta);
    }
    else
    {
        endpoint->mEndpoint.referenceCount = 0;
    }

    return mProvisioned.SetTableEntry(fabric, localId, endpoint->mEndpoint, endpoint->mBuffer);
}

} // namespace app
} // namespace chip
