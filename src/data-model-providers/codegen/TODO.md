# TODO: Buffer Adjustments for ServerClusterInterface to Ember Decoder

This document tracks necessary adjustments to global buffers and documentation of concerns regarding inline buffer usage for the `EmberAttributeDecoder`.

## 1. Buffer Adjustments Needed

### File: `src/app/util/ember-io-storage.cpp`
- **Location**: `kAttributeReadBufferSize` definition (around line 31).
- **Issue**: Currently, `kAttributeReadBufferSize` is sized based on `ATTRIBUTE_LARGEST` (generated from ZAP), with a minimum of 8 bytes.
  ```cpp
  constexpr size_t kAttributeReadBufferSize = (ATTRIBUTE_LARGEST >= 8 ? ATTRIBUTE_LARGEST : 8);
  ```
- **Required Change**: We need to ensure this buffer is large enough to hold the **TLV representation** of the largest attribute, not just its Ember representation. TLV encoding adds framing overhead (structure, array, path tags, etc.) that can make the encoded size larger than the raw data, especially for small attributes.
- **Concern**: If `gEmberAttributeIOBufferSpan` is used as scratch space for reading from `ServerClusterInterface` (which encodes to TLV), an 8-byte buffer will cause `CHIP_ERROR_BUFFER_TOO_SMALL` errors even for simple types like Booleans.

## 2. Inline Buffer Usage Concerns

In `EmberAttributeDecoder.cpp`, we use the provided `outBuffer` as scratch space for the intermediate TLV encoding to avoid large stack allocations. This raises the following concerns:

- **Minimum Size Requirement**: The caller must guarantee that `outBuffer` is large enough to hold the TLV overhead + data. If the caller passes a buffer sized exactly for the Ember representation (e.g., 1 byte for a Boolean), the TLV encoding will fail.
- **In-Place Decoding Overlap**: When decoding from TLV back to Ember format within the same buffer, we must ensure we don't overwrite data we haven't read yet.
    - For primitive types, the TLV representation is larger than the Ember representation, and the value is at the end, so the read pointer is ahead of the write pointer. This is generally safe.
    - For strings, we must be careful. `EmberAttributeDataBuffer::Decode` might use `memcpy` which is unsafe for overlapping regions. In `EmberAttributeDecoder.cpp`, we implemented string handling manually using `memmove` to support safe in-place decoding. Any future additions should follow this pattern or ensure `memmove` is used.
