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
#pragma once

#include <cstdint>

#include <app/clusters/tls-certificate-management-server/CertificateTable.h>
#include <app/clusters/tls-certificate-management-server/TLSCertificateManagementCluster.h>
#include <app/clusters/tls-client-management-server/TLSClientManagementCluster.h>
#include <app/data-model-provider/Provider.h>
#include <app/storage/FabricTableImpl.h>
#include <lib/core/CHIPError.h>
#include <lib/core/CHIPPersistentStorageDelegate.h>
#include <lib/core/DataModelTypes.h>
#include <protocols/interaction_model/StatusCode.h>

namespace chip {
namespace app {

inline constexpr uint16_t kUndefinedCodeDrivenTlsEndpointId = 0xffff;
inline constexpr uint8_t kMaxCodeDrivenTlsEndpoints         = 16;

struct CodeDrivenTlsEndpointId
{
    uint16_t mEndpointId = kUndefinedCodeDrivenTlsEndpointId;

    CodeDrivenTlsEndpointId() = default;
    explicit CodeDrivenTlsEndpointId(uint16_t id) : mEndpointId(id) {}

    uint16_t & Value() { return mEndpointId; }
    const uint16_t & Value() const { return mEndpointId; }

    void Clear() { mEndpointId = kUndefinedCodeDrivenTlsEndpointId; }

    bool IsValid() const { return (mEndpointId != kUndefinedCodeDrivenTlsEndpointId); }

    bool operator==(const CodeDrivenTlsEndpointId & other) const { return (mEndpointId == other.mEndpointId); }
    bool operator==(uint16_t other) const { return (mEndpointId == other); }
};

class CodeDrivenTlsEndpointTable
    : public Storage::FabricTableImpl<CodeDrivenTlsEndpointId, Clusters::TLSClientManagementDelegate::EndpointStructType>
{
public:
    using Super = Storage::FabricTableImpl<CodeDrivenTlsEndpointId, Clusters::TLSClientManagementDelegate::EndpointStructType>;

    CodeDrivenTlsEndpointTable() : Super(kMaxCodeDrivenTlsEndpoints, UINT16_MAX) {}
    ~CodeDrivenTlsEndpointTable() override { Finish(); }
};

/// Code-driven delegate for `TLSCertificateManagementCluster`, backed by an
/// injected `Tls::CertificateTable` without global singletons or ZAP dependencies.
class CodeDrivenTlsCertificateManagementDelegate : public Clusters::TLSCertificateManagementDelegate
{
public:
    explicit CodeDrivenTlsCertificateManagementDelegate(Clusters::Tls::CertificateTable & certificateTable,
                                                        EndpointId endpointId = kRootEndpointId) :
        mCertificateTable(certificateTable), mEndpointId(endpointId)
    {}
    ~CodeDrivenTlsCertificateManagementDelegate() override = default;

    void SetEndpoint(EndpointId endpointId) { mEndpointId = endpointId; }

    Protocols::InteractionModel::Status ProvisionRootCert(EndpointId matterEndpoint, FabricIndex fabric,
                                                          const ProvisionRootCertificateType & provisionReq,
                                                          Clusters::Tls::TLSCAID & outCaid) override;

    CHIP_ERROR LoadedRootCerts(EndpointId matterEndpoint, FabricIndex fabric,
                               LoadedRootCertificateCallback loadedCallback) const override;
    CHIP_ERROR RootCertsForFabric(EndpointId matterEndpoint, FabricIndex fabric,
                                  RootCertificateListCallback loadedCallback) const override;

    CHIP_ERROR FindRootCert(EndpointId matterEndpoint, FabricIndex fabric, Clusters::Tls::TLSCAID id,
                            LoadedRootCertificateCallback loadedCallback) const override;
    CHIP_ERROR LookupRootCertByFingerprint(EndpointId matterEndpoint, FabricIndex fabric, const ByteSpan & fingerprint,
                                           LoadedRootCertificateCallback loadedCallback) const override;
    CHIP_ERROR LookupRootCert(EndpointId matterEndpoint, FabricIndex fabric, const ByteSpan & certificate,
                              LoadedRootCertificateCallback loadedCallback) const override;
    Protocols::InteractionModel::Status RemoveRootCert(EndpointId matterEndpoint, FabricIndex fabric,
                                                       Clusters::Tls::TLSCAID id) override;

    Protocols::InteractionModel::Status GenerateClientCsr(EndpointId matterEndpoint, FabricIndex fabric,
                                                          const ClientCsrType & request,
                                                          GeneratedCsrCallback loadedCallback) const override;
    Protocols::InteractionModel::Status ProvisionClientCert(EndpointId matterEndpoint, FabricIndex fabric,
                                                            const ProvisionClientCertificateType & provisionReq) override;

    CHIP_ERROR LoadedClientCerts(EndpointId matterEndpoint, FabricIndex fabric,
                                 LoadedClientCertificateCallback loadedCallback) const override;
    CHIP_ERROR ClientCertsForFabric(EndpointId matterEndpoint, FabricIndex fabric,
                                    ClientCertificateListCallback loadedCallback) const override;

    CHIP_ERROR FindClientCert(EndpointId matterEndpoint, FabricIndex fabric, Clusters::Tls::TLSCCDID id,
                              LoadedClientCertificateCallback loadedCallback) const override;
    CHIP_ERROR LookupClientCertByFingerprint(EndpointId matterEndpoint, FabricIndex fabric, const ByteSpan & fingerprint,
                                             LoadedClientCertificateCallback loadedCallback) const override;
    CHIP_ERROR LookupClientCert(EndpointId matterEndpoint, FabricIndex fabric, const ByteSpan & certificate,
                                LoadedClientCertificateCallback loadedCallback) const override;
    Protocols::InteractionModel::Status RemoveClientCert(EndpointId matterEndpoint, FabricIndex fabric,
                                                         Clusters::Tls::TLSCCDID id) override;

private:
    Clusters::Tls::CertificateTable & mCertificateTable;
    EndpointId mEndpointId = kRootEndpointId;
};

/// Code-driven delegate for `TLSClientManagementCluster` and
/// `Tls::CertificateDependencyChecker`, with an injected `DataModel::Provider *`
/// for fabric removal cleanup.
class CodeDrivenTlsClientManagementDelegate : public Clusters::TLSClientManagementDelegate
{
public:
    explicit CodeDrivenTlsClientManagementDelegate(EndpointId endpointId = kRootEndpointId) : mEndpointId(endpointId) {}
    ~CodeDrivenTlsClientManagementDelegate() override = default;

    void SetEndpoint(EndpointId endpointId) { mEndpointId = endpointId; }
    void SetDataModelProvider(DataModel::Provider * provider) { mProvider = provider; }

    CHIP_ERROR Init(PersistentStorageDelegate & storage) override;

    CHIP_ERROR ForEachEndpoint(EndpointId matterEndpoint, FabricIndex fabric, LoadedEndpointCallback callback) override;

    Protocols::InteractionModel::ClusterStatusCode
    ProvisionEndpoint(EndpointId matterEndpoint, FabricIndex fabric,
                      const Clusters::TlsClientManagement::Commands::ProvisionEndpoint::DecodableType & provisionReq,
                      uint16_t & endpointID) override;

    CHIP_ERROR FindProvisionedEndpointByID(EndpointId matterEndpoint, FabricIndex fabric, uint16_t endpointID,
                                           LoadedEndpointCallback callback) override;

    Protocols::InteractionModel::Status RemoveProvisionedEndpointByID(EndpointId matterEndpoint, FabricIndex fabric,
                                                                      uint16_t endpointID) override;

    CHIP_ERROR RootCertCanBeRemoved(EndpointId matterEndpoint, FabricIndex fabric, Clusters::Tls::TLSCAID id) override;
    CHIP_ERROR ClientCertCanBeRemoved(EndpointId matterEndpoint, FabricIndex fabric, Clusters::Tls::TLSCCDID id) override;

    void RemoveFabric(FabricIndex fabric) override;

    CHIP_ERROR MutateEndpointReferenceCount(EndpointId matterEndpoint, FabricIndex fabric, uint16_t endpointID,
                                            int8_t delta) override;

private:
    CHIP_ERROR GetEndpointId(FabricIndex fabric, uint16_t & id);

    CodeDrivenTlsEndpointTable mProvisioned;
    PersistentStorageDelegate * mStorage = nullptr;
    DataModel::Provider * mProvider      = nullptr;
    EndpointId mEndpointId               = kRootEndpointId;
};

} // namespace app
} // namespace chip
