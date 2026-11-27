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
#include <device/types/root-node/features/TlsFeature.h>

#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>

using namespace chip::app::Clusters;

namespace chip {
namespace app {

CHIP_ERROR TlsFeature::RegisterFeatureClusters(EndpointId endpointId, CodeDrivenDataModelProvider & provider,
                                               RootNode::Context & rootContext, Clusters::GeneralCommissioningCluster &)
{
    if (!mContext.enabled)
    {
        return CHIP_NO_ERROR;
    }

    VerifyOrReturnError(mContext.maxRootCerts >= 5 && mContext.maxClientCerts >= 5 && mContext.maxEndpoints >= 5 &&
                            mContext.maxEndpoints <= kMaxCodeDrivenTlsEndpoints,
                        CHIP_ERROR_INVALID_ARGUMENT);

    ReturnErrorOnFailure(mCertificateTable.SetEndpoint(endpointId));
    mCertDelegate.SetEndpoint(endpointId);
    mClientDelegate.SetEndpoint(endpointId);
    mClientDelegate.SetDataModelProvider(&provider);

    TLSCertificateManagementCluster::Context certContext{ .fabricTable = rootContext.fabricTable };
    mTlsCertificateCluster.Create(certContext, endpointId, mCertDelegate, mClientDelegate, mCertificateTable, mContext.maxRootCerts,
                                  mContext.maxClientCerts);
    ReturnErrorOnFailure(provider.AddCluster(mTlsCertificateCluster.Registration()));

    TLSClientManagementCluster::Context clientContext{ .fabricTable = rootContext.fabricTable };
    mTlsClientCluster.Create(clientContext, endpointId, mClientDelegate, mCertificateTable, mContext.maxEndpoints);
    ReturnErrorOnFailure(provider.AddCluster(mTlsClientCluster.Registration()));

    return CHIP_NO_ERROR;
}

void TlsFeature::UnregisterFeatureClusters(CodeDrivenDataModelProvider & provider)
{
    if (mTlsClientCluster.IsConstructed())
    {
        LogErrorOnFailure(provider.RemoveCluster(&mTlsClientCluster.Cluster()));
        mTlsClientCluster.Destroy();
    }
    if (mTlsCertificateCluster.IsConstructed())
    {
        LogErrorOnFailure(provider.RemoveCluster(&mTlsCertificateCluster.Cluster()));
        mTlsCertificateCluster.Destroy();
    }
    mClientDelegate.SetDataModelProvider(nullptr);
}

} // namespace app
} // namespace chip
