/*
 *    Copyright (c) 2026 Project CHIP Authors
 *
 *    All rights reserved.
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

#include <app/ConcreteAttributePath.h>

namespace chip::app::DataModel {

enum class AttributeChangeType
{
    kReportable, // Change should be reported to subscribers
    kQuiet       // Change is minor or configured not to be reported
};

/// Interface for components wishing to be notified of attribute changes.
///
/// Implement this interface to receive callbacks when attributes are modified
/// within a DataModel::Provider. Listeners are registered with a specific
/// DataModel::Provider instance.
///
/// Notifications are:
/// - Called *after* the attribute state has been updated in the provider.
/// - Triggered for all attribute changes, regardless of whether they are
///   IM-reportable (i.e., includes kQuiet changes).
/// - Primarily used by the application layer to synchronize external state
///   (e.g., hardware) with the new attribute value.
/// - Synchronous: Called inline during the attribute update process.
///   Implementations should be wary of re-entrancy or recursive calls if
///   they modify cluster state within the callback.
/// - Fire-and-forget: There is no mechanism to report failure back to the
///   caller. If an action taken in the callback fails (e.g., hardware
///   actuation), the listener is responsible for any corrective measures,
///   such as reverting the attribute state in the DataModel::Provider.
class AttributeChangeListener
{
public:
    virtual ~AttributeChangeListener() = default;

    /// Called after an attribute's value has changed.
    virtual void OnAttributeChanged(const ConcreteAttributePath & path, AttributeChangeType type) = 0;

    AttributeChangeListener * GetNextAttributeChangeListener() const { return mNextAttributeChange; }
    void SetNextAttributeChangeListener(AttributeChangeListener * next) { mNextAttributeChange = next; }

private:
    AttributeChangeListener * mNextAttributeChange = nullptr;
};

} // namespace chip::app::DataModel
