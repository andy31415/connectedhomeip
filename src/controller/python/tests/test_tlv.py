#
#    Copyright (c) 2021 Project CHIP Authors
#    All rights reserved.
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

import unittest

from matter.tlv import TLVList, TLVReader, TLVWriter
from matter.tlv import uint as tlvUint


class TestTLVWriter(unittest.TestCase):
    def _getEncoded(self, val, tag=None):
        writer = TLVWriter()
        writer.put(tag, val)
        return writer.encoding

    def test_int(self):
        encodedVal = self._getEncoded(0x00DEADBEEFCA00FE)
        self.assertEqual(encodedVal, bytearray([0b00000011, 0xFE, 0x00, 0xCA, 0xEF, 0xBE, 0xAD, 0xDE, 0x00]))
        encodedVal = self._getEncoded(0x7CADBEEF)
        self.assertEqual(encodedVal, bytearray([0b00000010, 0xEF, 0xBE, 0xAD, 0x7C]))
        encodedVal = self._getEncoded(0x7CAD)
        self.assertEqual(encodedVal, bytearray([0b00000001, 0xAD, 0x7C]))
        encodedVal = self._getEncoded(0x7C)
        self.assertEqual(encodedVal, bytearray([0b00000000, 0x7C]))
        # Negative numbers
        encodedVal = self._getEncoded(-(0x5555555555555555))
        self.assertEqual(encodedVal, bytearray([0b00000011, 0xAB, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA]))
        encodedVal = self._getEncoded(-(0x55555555))
        self.assertEqual(encodedVal, bytearray([0b00000010, 0xAB, 0xAA, 0xAA, 0xAA]))
        encodedVal = self._getEncoded(-(0x5555))
        self.assertEqual(encodedVal, bytearray([0b00000001, 0xAB, 0xAA]))
        encodedVal = self._getEncoded(-(0x55))
        self.assertEqual(encodedVal, bytearray([0b00000000, 0xAB]))
        # The following numbers are positive values but exceeds the upper bounds of the type they seems to be.
        encodedVal = self._getEncoded(0xDEADBEEF)
        self.assertEqual(encodedVal, bytearray([0b00000011, 0xEF, 0xBE, 0xAD, 0xDE, 0x00, 0x00, 0x00, 0x00]))
        encodedVal = self._getEncoded(0xDEAD)
        self.assertEqual(encodedVal, bytearray([0b00000010, 0xAD, 0xDE, 0x00, 0x00]))
        encodedVal = self._getEncoded(0xAD)
        self.assertEqual(encodedVal, bytearray([0b00000001, 0xAD, 0x00]))
        # Similar, these negative numbers also exceedes the width of the type they seems to be.
        encodedVal = self._getEncoded(-(0xAAAAAAAA))
        self.assertEqual(encodedVal, bytearray([0b00000011, 0x56, 0x55, 0x55, 0x55, 0xFF, 0xFF, 0xFF, 0xFF]))
        encodedVal = self._getEncoded(-(0xAAAA))
        self.assertEqual(encodedVal, bytearray([0b00000010, 0x56, 0x55, 0xFF, 0xFF]))
        encodedVal = self._getEncoded(-(0xAA))
        self.assertEqual(encodedVal, bytearray([0b00000001, 0x56, 0xFF]))
        try:
            encodedVal = self._getEncoded(0xF000F000F000F000)
            self.fail("Signed number exceeds INT64_MAX but no exception received")
        except Exception:
            pass

        try:
            encodedVal = self._getEncoded(-(0xF000F000F000F000))
            self.fail("Signed number exceeds INT64_MIN but no exception received")
        except Exception:
            pass

    def test_uint(self):
        encodedVal = self._getEncoded(tlvUint(0xDEADBEEFCA0000FE))
        self.assertEqual(encodedVal, bytearray([0b00000111, 0xFE, 0x00, 0x00, 0xCA, 0xEF, 0xBE, 0xAD, 0xDE]))
        encodedVal = self._getEncoded(tlvUint(0xDEADBEEF))
        self.assertEqual(encodedVal, bytearray([0b00000110, 0xEF, 0xBE, 0xAD, 0xDE]))
        encodedVal = self._getEncoded(tlvUint(0xDEAD))
        self.assertEqual(encodedVal, bytearray([0b00000101, 0xAD, 0xDE]))
        encodedVal = self._getEncoded(tlvUint(0xDE))
        self.assertEqual(encodedVal, bytearray([0b00000100, 0xDE]))
        try:
            encodedVal = self._getEncoded(tlvUint(-1))
            self.fail("Negative unsigned int but no exception raised.")
        except Exception:
            pass
        try:
            encodedVal = self._getEncoded(tlvUint(0x10000000000000000))
            self.fail("Overflowed uint but no exception raised.")
        except Exception:
            pass

    def test_list(self):
        encodeVal = self._getEncoded(TLVList([(None, 1), (None, 2), (1, 3)]))
        self.assertEqual(
            encodeVal,
            bytearray(
                [
                    0b00010111,  # List, anonymous tag
                    0x00,
                    0x01,  # Anonymous tag, 1 octet signed int `1``
                    0x00,
                    0x02,  # Anonymous tag, 1 octet signed int `2``
                    0b00100000,
                    0x01,
                    0x03,  # Context specific tag `1`, 1 octet signed int `3`
                    0x18,  # End of container
                ]
            ),
        )
        encodeVal = self._getEncoded(TLVList([(None, 1), (None, TLVList([(None, 2), (3, 4)]))]))
        self.assertEqual(
            encodeVal,
            bytearray(
                [
                    0b00010111,  # List, anonymous tag
                    0x00,
                    0x01,  # Anonymous tag, 1 octet signed int `1``
                    0b00010111,  # List anonymous tag
                    0x00,
                    0x02,  # Anonymous tag, 1 octet signed int `2``
                    0b00100000,
                    0x03,
                    0x04,  # Context specific tag `1`, 1 octet signed int `3`
                    0x18,  # End of inner list
                    0x18,  # End of container
                ]
            ),
        )


class TestTLVReader(unittest.TestCase):
    def _read_case(self, input, answer):
        decoded = TLVReader(bytearray(input)).get()["Any"]
        self.assertEqual(type(decoded), type(answer))
        self.assertEqual(decoded, answer)

    def test_int(self):
        self._read_case([0b00000011, 0xFE, 0x00, 0xCA, 0xEF, 0xBE, 0xAD, 0xDE, 0x00], 0x00DEADBEEFCA00FE)
        self._read_case([0b00000010, 0xEF, 0xBE, 0xAD, 0x7C], 0x7CADBEEF)
        self._read_case([0b00000001, 0xAD, 0x7C], 0x7CAD)
        self._read_case([0b00000000, 0x7C], 0x7C)

        self._read_case([0b00000011, 0xAB, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA], -(0x5555555555555555))
        self._read_case([0b00000010, 0xAB, 0xAA, 0xAA, 0xAA], -(0x55555555))
        self._read_case([0b00000001, 0xAB, 0xAA], -(0x5555))
        self._read_case([0b00000000, 0xAB], -(0x55))

    def test_uint(self):
        self._read_case([0b00000111, 0xFE, 0x00, 0xCA, 0xEF, 0xBE, 0xAD, 0xDE, 0x00], tlvUint(0x00DEADBEEFCA00FE))
        self._read_case([0b00000110, 0xEF, 0xBE, 0xAD, 0x7C], tlvUint(0x7CADBEEF))
        self._read_case([0b00000101, 0xAD, 0x7C], tlvUint(0x7CAD))
        self._read_case([0b00000100, 0x7C], tlvUint(0x7C))

        self._read_case([0b00000111, 0xAB, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA], tlvUint(0xAAAAAAAAAAAAAAAB))
        self._read_case([0b00000110, 0xAB, 0xAA, 0xAA, 0xAA], tlvUint(0xAAAAAAAB))
        self._read_case([0b00000101, 0xAB, 0xAA], tlvUint(0xAAAB))
        self._read_case([0b00000100, 0xAB], tlvUint(0xAB))

    def test_structure(self):
        test_cases = [
            (
                b"\x15\x36\x01\x15\x35\x01\x26\x00\xbf\xa2\x55\x16\x37\x01\x24"
                b"\x02\x00\x24\x03\x28\x24\x04\x00\x18\x24\x02\x01\x18\x18\x18\x18",
                {1: [{1: {0: 374710975, 1: TLVList([(2, 0), (3, 40), (4, 0)]), 2: 1}}]},
            ),
            (
                b"\x156\x01\x155\x01&\x00\xbf\xa2U\x167\x01$\x02\x00$\x03($\x04\x01"
                b"\x18,\x02\x18Nordic Semiconductor ASA\x18\x18\x18\x18",
                {1: [{1: {0: 374710975, 1: TLVList([(2, 0), (3, 40), (4, 1)]), 2: "Nordic Semiconductor ASA"}}]},
            ),
            (
                b"\0256\001\0255\001&\000\031\346x\2077\001$\002\001$\003\006$\004\000\030(\002\030\030\030\030",
                {1: [{1: {0: 2272847385, 1: TLVList([(2, 1), (3, 6), (4, 0)]), 2: False}}]},
            ),
        ]
        for tlv_bytes, answer in test_cases:
            self._read_case(tlv_bytes, answer)

    def test_list(self):
        self._read_case(
            [
                0b00010111,  # List, anonymous tag
                0x00,
                0x01,  # Anonymous tag, 1 octet signed int `1``
                0x00,
                0x02,  # Anonymous tag, 1 octet signed int `2``
                0b00100000,
                0x01,
                0x03,  # Context specific tag `1`, 1 octet signed int `3`
                0x18,  # End of container
            ],
            TLVList([(None, 1), (None, 2), (1, 3)]),
        )
        self._read_case(
            [
                0b00010111,  # List, anonymous tag
                0x00,
                0x01,  # Anonymous tag, 1 octet signed int `1``
                0b00010111,  # List anonymous tag
                0x00,
                0x02,  # Anonymous tag, 1 octet signed int `2``
                0b00100000,
                0x03,
                0x04,  # Context specific tag `1`, 1 octet signed int `3`
                0x18,  # End of inner list
                0x18,  # End of container
            ],
            TLVList([(None, 1), (None, TLVList([(None, 2), (3, 4)]))]),
        )


class TestTLVTypes(unittest.TestCase):
    def test_list(self):
        var = TLVList([(None, 1), (None, 2), (1, 3)])
        self.assertEqual(var[1], 3)
        self.assertEqual(var[TLVList.IndexMethod.Index : 0], (None, 1))
        self.assertEqual(var[TLVList.IndexMethod.Tag : 1], 3)

        var.append(None, 4)
        self.assertEqual(var, TLVList([(None, 1), (None, 2), (1, 3), (None, 4)]))

        var.append(5, 6)
        self.assertEqual(var, TLVList([(None, 1), (None, 2), (1, 3), (None, 4), (5, 6)]))

        expectIterateContent = [(None, 1), (None, 2), (1, 3), (None, 4), (5, 6)]
        iteratedContent = []
        for tag, value in var:
            iteratedContent.append((tag, value))
        self.assertEqual(expectIterateContent, iteratedContent)


if __name__ == "__main__":
    unittest.main()
