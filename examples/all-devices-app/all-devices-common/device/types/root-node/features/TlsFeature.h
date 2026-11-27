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

#include <array>
#include <cstdint>

#include <app/clusters/tls-certificate-management-server/CertificateTableImpl.h>
#include <app/clusters/tls-certificate-management-server/TLSCertificateManagementCluster.h>
#include <app/clusters/tls-client-management-server/TLSClientManagementCluster.h>
#include <app/server-cluster/ServerClusterInterfaceRegistry.h>
#include <data-model-providers/codedriven/CodeDrivenDataModelProvider.h>
#include <device/types/root-node/RootNode.h>
#include <device/types/root-node/features/CodeDrivenTlsDelegates.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>

namespace chip {
namespace app {

/// Feature policy that adds `TLSCertificateManagement` and
/// `TLSClientManagement` clusters to the root endpoint, sharing one
/// `Tls::CertificateTableImpl` and dependency checker.
///
/// When `Context::enabled` is false, registration is a no-op so compositions
/// can opt in at runtime.
class TlsFeature
{
public:
    struct Context
    {
        bool enabled           = true;
        uint8_t maxRootCerts   = 5;
        uint8_t maxClientCerts = 5;
        uint8_t maxEndpoints   = 5;
    };

    explicit TlsFeature(const Context & context) : mContext(context) {}

    static constexpr std::array<DataModel::DeviceTypeEntry, 0> kExtraDeviceTypes{};
    static constexpr std::array<ClusterId, 0> kExtraClientClusters{};

    CHIP_ERROR RegisterFeatureClusters(EndpointId endpointId, CodeDrivenDataModelProvider & provider,
                                       RootNode::Context & rootContext,
                                       Clusters::GeneralCommissioningCluster & generalCommissioning);

    void UnregisterFeatureClusters(CodeDrivenDataModelProvider & provider);

    bool IsEnabled() const { return mContext.enabled; }
    Clusters::TLSCertificateManagementCluster & CertificateCluster() { return mTlsCertificateCluster.Cluster(); }
    Clusters::TLSClientManagementCluster & ClientCluster() { return mTlsClientCluster.Cluster(); }
    Clusters::TLSCertificateManagementDelegate & CertificateDelegate() { return mCertDelegate; }
    Clusters::TLSClientManagementDelegate & ClientDelegate() { return mClientDelegate; }
    Clusters::Tls::CertificateTable & CertificateTable() { return mCertificateTable; }

private:
    Context mContext;
    Clusters::Tls::CertificateTableImpl mCertificateTable;
    CodeDrivenTlsCertificateManagementDelegate mCertDelegate{ mCertificateTable };
    CodeDrivenTlsClientManagementDelegate mClientDelegate;
    LazyRegisteredServerCluster<Clusters::TLSCertificateManagementCluster> mTlsCertificateCluster;
    LazyRegisteredServerCluster<Clusters::TLSClientManagementCluster> mTlsClientCluster;
};

} // namespace app
} // namespace chip
