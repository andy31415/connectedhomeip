/*
 *
 *    Copyright (c) 2023 Project CHIP Authors
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

#include <AppMain.h>
#include <semaphore.h>

namespace example {
namespace Ui {

/**
 * Supports showing a UI using ImGUI
 *
 * The UI supports several windows, such as QR codes or device control.
 */
class ImguiUi : public AppMainLoopImplementation
{
public:
    ImguiUi() { sem_init(&mChipLoopWaitSemaphore, 0 /* shared */, 0); }
    virtual ~ImguiUi() { sem_destroy(&mChipLoopWaitSemaphore); }

    // FIXME: add windows

    void UpdateState(); // schedules a state update from ember/app
    void Render();      // render windows to screen

    // AppMainLoopImplementation
    void RunMainLoop() override;
    void SignalSafeStopMainLoop() override;

private:
    sem_t mChipLoopWaitSemaphore; // lock to wait for app state update

    // First initial state load
    void ChipLoopLoadInitialState();

    // Updates the window states. Run in the CHIP main loop (has access
    // to CHIP API calls)
    void ChipLoopStateUpdate();

    // Run in CHIPMainLoop to allow access of ember variables
    static void ChipLoopUpdateCallback(intptr_t self);
};

} // namespace Ui
} // namespace example