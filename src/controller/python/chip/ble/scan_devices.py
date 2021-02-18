#
#    Copyright (c) 2021 Project CHIP Authors
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

import ctypes
from chip.ble.library_handle import _GetBleLibraryHandle
from chip.ble.types import DeviceScannedCallback_t, ScanDoneCallback_t


@DeviceScannedCallback_t
def ScanFoundCallback(closure, address: str, discriminator: int, vendor: int,
                      product: int):
  closure.DeviceFound(address, discriminator, vendor, product)


@ScanDoneCallback_t
def ScanDoneCallback(closure):
  closure.ScanCompleted()


def Discover(timeoutMs: int, scanCallback, doneCallback, adapter=None):
  """Initiate a BLE discovery of devices with the given timeout.

  :param timeoutMs    - scan will complete after this time
  :param scanCallback - callback when a device is found
  :param doneCallback - callback when the scan is complete
  :param adapter      - what adapter to choose. Either an AdapterInfo object or
                        a string with the adapter address. If None, the first
                        adapter on the system is used.
  """
  if adapter and not isinstance(adapter, str):
    adapter = adapter.address

  handle = _GetBleLibraryHandle()

  nativeList = handle.pychip_ble_adapter_list_new()
  if nativeList == 0:
    raise Exception('Failed to list available adapters')

  try:
    while handle.pychip_ble_adapter_list_next(nativeList):
      if adapter and (adapter != handle.pychip_ble_adapter_list_get_address(
          nativeList).decode('utf8')):
        continue

      class ScannerClosure:

        def DeviceFound(self, *args):
          scanCallback(*args)

        def ScanCompleted(self, *args):
          doneCallback(*args)

      scanner = handle.pychip_ble_start_scanning(
          ctypes.py_object(ScannerClosure()),
          handle.pychip_ble_adapter_list_get_raw_adapter(nativeList), timeoutMs,
          ScanFoundCallback, ScanDoneCallback)

      if scanner == 0:
        raise Exception('Failed to initiate scan')
      break
  finally:
    handle.pychip_ble_adapter_list_delete(nativeList)
