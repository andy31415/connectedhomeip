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

#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>

namespace chip::app::Clusters::OnOff {

/**
 * @brief Interface for interacting with the Scenes cluster (or Scene Table).
 *
 * Used by OnOffLightingCluster to implement Global Scene Control logic.
 */
class SceneInteractor
{
public:
    virtual ~SceneInteractor() = default;

    virtual CHIP_ERROR StoreCurrentScene(FabricIndex fabricIndex, GroupId groupId, SceneId sceneId) = 0;
    virtual CHIP_ERROR RecallScene(FabricIndex fabricIndex, GroupId groupId, SceneId sceneId)       = 0;
};

} // namespace chip::app::Clusters::OnOff
