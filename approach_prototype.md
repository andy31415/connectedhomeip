# Prototype: Attribute Change Callback Compatibility — Call Sequencing

This document details the proposed implementation for `callback_compatibility_analysis.md` (Approach 4), separating pre-change and post-change concerns and tracing exact call sequences and code-level changes needed.

---

## Prerequisite: Small SDK Changes Required

Before either path can work, two minor things are needed:

1. **`AttributeValueDecoder` needs a public TLV reader accessor.**
   Currently `mReader` is private with only `friend class TestOnlyAttributeValueDecoderAccessor`. Adding a `GetTLVReader()` returning a copy (by value — `TLVReader` is copyable) exposes the reader for peeking without advancing the original:

   ```cpp
   // src/app/AttributeValueDecoder.h
   TLV::TLVReader GetTLVReader() const { return mReader; }  // returns a copy
   ```

2. **`DataModel::Provider` needs a new virtual notification method** for internal cluster attribute changes (detailed in §3). Default implementation is a no-op, so existing providers are unaffected.

---

## 1. Pre-Change Callback (`MatterPreAttributeChangeCallback`)

### 1.1 Legacy Behavior

From `src/app/util/attribute-table.cpp`, the exact sequence in `emAfWriteAttribute` is:

```
1. Validate writable, access control, range checks
2. AttributeValueIsChanging() → if NOT changing: return Success (no callbacks)
3. MatterPreAttributeChangeCallback(path, type, size, newValuePtr)  ← only if changing
4. emAfClusterPreAttributeChangedCallback(...)                       ← cluster-specific
5. emAfReadOrWriteAttribute(... write=true)                          ← write storage
6. emberAfAttributeChanged(...)                                      ← mark dirty
7. MatterPostAttributeChangeCallback(path, type, size, newValuePtr)
8. emAfClusterAttributeChangedCallback(...)
```

The pre-callback receives the **new value** (before it is written) and can reject it.

### 1.2 Injection Point

`CodegenDataModelProvider::WriteAttribute` already fetches `EmberAfAttributeMetadata` for all paths at line 86 (it's present for code-driven clusters registered via ZAP). The injection point is between the metadata fetch and `cluster->WriteAttribute()`:

```cpp
// Current code (CodegenDataModelProvider_Write.cpp ~line 109):
if (auto * cluster = mRegistry.Get(request.path); cluster != nullptr)
{
    return cluster->WriteAttribute(request, decoder);
}

// Proposed:
if (auto * cluster = mRegistry.Get(request.path); cluster != nullptr)
{
    if (attributeMetadata != nullptr)
    {
        // Peek the incoming value without consuming the TLV cursor.
        // Requires AttributeValueDecoder::GetTLVReader() (see §Prerequisite).
        TLV::TLVReader peekReader = decoder.GetTLVReader();

        // Reuse the global IO buffer — already sized to ATTRIBUTE_LARGEST,
        // which covers the largest possible Ember string or scalar.
        MutableByteSpan dataBuffer = gEmberAttributeIOBufferSpan;
        Ember::EmberAttributeDataBuffer emberData(attributeMetadata, dataBuffer);

        if (emberData.Decode(peekReader) == CHIP_NO_ERROR)
        {
            Status preStatus = MatterPreAttributeChangeCallback(
                request.path,
                attributeMetadata->attributeType,
                emberAfAttributeSize(attributeMetadata),
                dataBuffer.data());

            if (preStatus != Status::Success)
                return preStatus;
        }
        // If Decode fails (malformed TLV), skip the pre-callback and let the
        // cluster reject the write in its own validation.
    }

    return cluster->WriteAttribute(request, decoder);
}
```

### 1.3 Known Behavioral Differences from Legacy

**Value-changed check is absent.**
The legacy pre-callback fires only when the value actually changes (guarded by `AttributeValueIsChanging`). For code-driven clusters, the cluster performs this check internally during `WriteAttribute` — but we invoke the pre-callback *before* the cluster runs, so we cannot know yet whether the write is a no-op.

Options:
- **Always call pre-callback** (even for no-op writes). Apps that rely on "pre-callback implies a real change" will see spurious calls. In practice, most pre-callback implementations just validate the incoming value or return `Success` unconditionally, so spurious calls are harmless but not ideal.
- **Read-before-compare**: call `cluster->ReadAttribute` before the write, compare with incoming TLV, skip pre-callback if identical. Expensive (an extra read + TLV comparison for every write).
- **Cluster signals no-change**: if `cluster->WriteAttribute` returns a dedicated "value unchanged" status, call the pre-callback speculatively, then suppress the post-callback if the cluster signals no change. This requires a new return value from `WriteAttribute`.

The simplest prototype: always call the pre-callback. Document the difference. Re-evaluate based on real-world callback behaviour in existing apps.

**Timer-driven / internal writes do not trigger the pre-callback.**
In the legacy Ember path, internal writes (e.g., a level-control timer calling `emberAfWriteAttribute`) also went through `emAfWriteAttribute` and triggered the pre-callback. For code-driven clusters, internal attribute updates bypass `WriteAttribute` entirely.

The pre-callback is defined as rejectable — an app can block a write. Internal cluster writes are not rejectable by design (the cluster is acting on its own logic). This is a semantic difference that is arguably *correct* for the new model: the pre-callback is only meaningful on externally-sourced writes. This should be documented explicitly as a breaking change.

### 1.4 Call Sequence (External Write)

```
Caller → CodegenDataModelProvider::WriteAttribute(request, decoder)
  │
  ├─ emberAfLocateAttributeMetadata(path) → attributeMetadata (has type, size)
  │
  ├─ mRegistry.Get(path) → cluster (code-driven)
  │
  ├─ [NEW] decoder.GetTLVReader() → peekReader (copy, cursor at value)
  ├─ [NEW] EmberAttributeDataBuffer::Decode(peekReader) → gEmberAttributeIOBufferSpan
  ├─ [NEW] MatterPreAttributeChangeCallback(path, type, size, buf)
  │         └─ returns non-Success → return rejection status (cluster NOT called)
  │
  └─ cluster->WriteAttribute(request, decoder)
       └─ cluster decodes, validates, commits value
       └─ returns ActionReturnStatus
```

---

## 2. Post-Change Callback — External Writes

### 2.1 Injection Point

Same location in `CodegenDataModelProvider::WriteAttribute`, after the cluster returns success:

```cpp
DataModel::ActionReturnStatus result = cluster->WriteAttribute(request, decoder);

if (result.IsSuccess() && attributeMetadata != nullptr)
{
    // Read back the committed value from the cluster.
    // See §2.2 for the two options here.
    CallPostAttributeChangeCallback(request.path, *cluster, *attributeMetadata);
}

return result;
```

### 2.2 Getting the Post-Change Value — Two Options

**Option A — Read-back via `StackTLVEncoder`**

Create a new lightweight `StackTLVEncoder` class that implements the same `Encode()` template interface as `AttributeValueEncoder` but writes to a caller-provided buffer via a bare `TLVWriter` (no `AttributeReportIBs::Builder`, no list chunking, no fabric filtering):

```cpp
// New class: src/app/StackTLVEncoder.h (or similar small file)
class StackTLVEncoder {
    TLV::TLVWriter mWriter;
    uint8_t * mBuf;
public:
    StackTLVEncoder(uint8_t * buf, size_t len) : mBuf(buf) { mWriter.Init(buf, len); }

    template <typename T>
    CHIP_ERROR Encode(const T & value) { return DataModel::Encode(mWriter, TLV::AnonymousTag(), value); }

    TLV::TLVReader GetReader() {
        TLV::TLVReader r;
        r.Init(mBuf, mWriter.GetLengthWritten());
        return r;
    }
};
```

Post-change call sequence:
```cpp
void CallPostAttributeChangeCallback(const ConcreteAttributePath & path,
                                     ServerClusterInterface & cluster,
                                     const EmberAfAttributeMetadata & metadata)
{
    // Two-buffer approach: TLV into a local stack buffer, then decode to gEmberAttributeIOBufferSpan.
    // For scalars: TLV is at most 9 bytes.
    // For strings: TLV is string length + ~3 bytes overhead — same size as the Ember buffer needed.
    // Limitation: requires knowing a safe upper bound for the TLV buffer at compile time.
    // Use metadata.size + kTlvOverhead as the bound.
    const size_t tlvBufSize = metadata.size + 8; // generous for TLV framing
    uint8_t tlvBuf[tlvBufSize]; // VLA or fixed max — see note below
    StackTLVEncoder encoder(tlvBuf, sizeof(tlvBuf));

    DataModel::ReadAttributeRequest readReq;
    readReq.path = path;
    if (cluster.ReadAttribute(readReq, encoder) != ActionReturnStatus::kSuccess) return;

    TLV::TLVReader reader = encoder.GetReader();
    MutableByteSpan emberBuf = gEmberAttributeIOBufferSpan;
    Ember::EmberAttributeDataBuffer emberData(&metadata, emberBuf);
    if (emberData.Decode(reader) != CHIP_NO_ERROR) return;

    MatterPostAttributeChangeCallback(path, metadata.attributeType,
                                       emberAfAttributeSize(&metadata), emberBuf.data());
}
```

**Problem with stack buffer sizing**: `metadata.size` is a runtime value. A C99 VLA or `alloca` is not portable in C++. The alternative is using a fixed compile-time maximum (`ATTRIBUTE_LARGEST`), but that may be large (e.g., 256+ bytes). For strings, we cannot escape this allocation — the string bytes must live somewhere during the callback.

One mitigating approach: allocate the TLV buffer from `gEmberAttributeIOBufferSpan` itself (we're in a write path, so it's not being used for anything else at this point), then decode in-place. The TLV encoding of a string attribute is very similar in size to its Ember representation, so a double-use of the buffer with careful sequencing may work.

**Option C — Cluster pushes an `AttributeRawValue` descriptor**

The cluster calls a notification at commit time. The provider receives the raw pointer + size + kind, validates against metadata, and calls the callback:

```cpp
struct AttributeRawValue {
    const void * data;
    size_t size;
    bool isString;
};

// In DataModel::Provider (new virtual, default no-op):
virtual void NotifyAttributeValueChanged(const ConcreteAttributePath & path,
                                         const AttributeRawValue & value) {}
```

`CodegenDataModelProvider` overrides this:
```cpp
void CodegenDataModelProvider::NotifyAttributeValueChanged(const ConcreteAttributePath & path,
                                                            const AttributeRawValue & value)
{
    const EmberAfAttributeMetadata * metadata =
        emberAfLocateAttributeMetadata(path.mEndpointId, path.mClusterId, path.mAttributeId);
    if (metadata == nullptr) return;

    // Validate
    if (!value.isString && value.size != metadata->size) return; // scalar size mismatch
    if (value.isString && value.size > metadata->size) return;   // string too long

    MutableByteSpan emberBuf = gEmberAttributeIOBufferSpan;

    if (!value.isString)
    {
        // Scalars: C++ member is already native-endian = Ember format. Direct copy.
        memcpy(emberBuf.data(), value.data, value.size);
    }
    else
    {
        // Strings: prepend Pascal length prefix.
        emberBuf.data()[0] = static_cast<uint8_t>(value.size);
        memcpy(emberBuf.data() + 1, value.data, value.size);
    }

    MatterPostAttributeChangeCallback(path, metadata->attributeType,
                                       emberAfAttributeSize(metadata), emberBuf.data());
}
```

Cluster call site (at commit time in `WriteAttribute`):
```cpp
// After: mOnOff = newValue;
context.provider.NotifyAttributeValueChanged(path, { &mOnOff, sizeof(mOnOff), false });
```

No extra read, no TLV, no stack buffer problem. The cluster points directly into its own storage.

### 2.3 Call Sequence (External Write, Option C)

```
Caller → CodegenDataModelProvider::WriteAttribute(request, decoder)
  │
  ├─ [pre-change block — see §1]
  │
  ├─ cluster->WriteAttribute(request, decoder)
  │    └─ decodes TLV, commits value to mMyAttribute
  │    └─ context.provider.NotifyAttributeValueChanged(path, {&mMyAttribute, size, false})
  │         └─ CodegenDataModelProvider::NotifyAttributeValueChanged(...)
  │              └─ validates size vs. metadata
  │              └─ copies to gEmberAttributeIOBufferSpan
  │              └─ MatterPostAttributeChangeCallback(path, type, size, buf)
  │
  └─ return result
```

Note: with Option C, the post-callback fires from *within* `cluster->WriteAttribute`, before it returns. This matches the legacy Ember ordering (post-callback before function return). With Option A (read-back after return), the post-callback fires after the cluster returns.

---

## 3. Post-Change Callback — Internal Cluster Updates

This is the more critical and harder case. A cluster updating its own state (timer, command side-effect, initialization) never goes through `CodegenDataModelProvider::WriteAttribute`. The current notification path is:

```
cluster → context.provider.Temporary_ReportAttributeChanged(path)
               → emberAfAttributeChanged(...)
                    → MarkDirty for subscription reporting
```

`MatterPostAttributeChangeCallback` is never called on this path.

### 3.1 Proposed Extension

Add `NotifyAttributeValueChanged` to `DataModel::Provider` (as introduced in §2.2, Option C). The cluster calls it alongside or instead of `Temporary_ReportAttributeChanged`:

```cpp
// In the cluster, when a timer fires and updates CurrentLevel:
mCurrentLevel = newLevel;
context.provider.Temporary_ReportAttributeChanged({endpoint, LevelControl::Id, CurrentLevel::Id});
context.provider.NotifyAttributeValueChanged(
    {endpoint, LevelControl::Id, CurrentLevel::Id},
    {&mCurrentLevel, sizeof(mCurrentLevel), false});
```

Or the two calls could be merged into a single helper that clusters use for any attribute update, combining reporting and legacy callback notification:

```cpp
// Helper wrapping both concerns:
void ServerClusterInterface::MarkAttributeChanged(const ConcreteAttributePath & path,
                                                  const AttributeRawValue & value)
{
    context.provider.Temporary_ReportAttributeChanged(path);
    context.provider.NotifyAttributeValueChanged(path, value);
}
```

### 3.2 Problem: Opt-In at Every Call Site

Any cluster that does not call `NotifyAttributeValueChanged` at every internal attribute mutation will silently miss post-callbacks. There is no compile-time check for this. The per-cluster burden is real and omissions will not be caught by the type system.

Mitigation options:
- Code review / documentation: "clusters must call `NotifyAttributeValueChanged` for all attribute changes to maintain legacy callback compatibility."
- Default base class helper in `DefaultServerCluster` that makes the combined call the default path.
- Lint or static analysis rule (hard to enforce generically).

### 3.3 Call Sequence (Internal Update)

```
Timer fires → cluster->OnTimerFired()
  │
  ├─ mCurrentLevel = newLevel
  │
  ├─ context.provider.Temporary_ReportAttributeChanged(path)   ← subscription reporting
  │
  └─ context.provider.NotifyAttributeValueChanged(path, value) ← legacy app callbacks
       └─ CodegenDataModelProvider::NotifyAttributeValueChanged(...)
            └─ validates, converts to Ember format
            └─ MatterPostAttributeChangeCallback(path, type, size, buf)
```

---

## 4. Recursive Callback Risk

`MatterPostAttributeChangeCallback` may itself write an attribute (e.g., sync a derived state). If that write goes through `CodegenDataModelProvider::WriteAttribute`, it will again call `MatterPreAttributeChangeCallback` and, on success, `MatterPostAttributeChangeCallback` — recursion.

On embedded targets with 4–8 kB stacks this is dangerous. The implementation must guard against it:

```cpp
// In CodegenDataModelProvider:
bool mInAttributeCallback = false;

void CodegenDataModelProvider::NotifyAttributeValueChanged(...)
{
    if (mInAttributeCallback) return; // depth guard: suppress inner notifications
    mInAttributeCallback = true;
    // ... call MatterPostAttributeChangeCallback ...
    mInAttributeCallback = false;
}
```

This silently drops inner callbacks. See `callback_compatibility_analysis.md` §3.F for alternative policies (prohibition, deferred queue). The depth guard is the safest default for a prototype.

---

## 5. Summary of Required Code Changes

| Component | Change | Scope |
|---|---|---|
| `src/app/AttributeValueDecoder.h` | Add `GetTLVReader() const` returning a copy | Small, safe |
| `src/app/data-model-provider/Provider.h` | Add `virtual void NotifyAttributeValueChanged(path, AttributeRawValue)` with no-op default | Non-breaking |
| `src/data-model-providers/codegen/CodegenDataModelProvider.h/.cpp` | Override `NotifyAttributeValueChanged`; add pre/post logic to `WriteAttribute` | Core change |
| `src/app/StackTLVEncoder.h` | New lightweight encoder class (Option A only) | New file, small |
| Each code-driven cluster | Call `NotifyAttributeValueChanged` at every internal attribute commit | Per-cluster boilerplate |

---

## 6. Open Issues for the Prototype

1. **Pre-callback on no-op writes**: Always-call is simplest. Before finalising, survey existing pre-callback implementations in `examples/` to check if any rely on "only fires on actual change" semantics.

2. **Option A vs Option C for post-change value**:
   - Option A (`StackTLVEncoder`): zero cluster changes, but stack buffer sizing for large strings is unresolved. VLAs are non-portable; fixed max (`ATTRIBUTE_LARGEST`) is safe but potentially wasteful.
   - Option C (`AttributeRawValue`): no buffer sizing issue, direct pointer, provider-side validation. Requires per-cluster call sites.

3. **Pre-callback for internal writes**: currently excluded by design. Confirm with stakeholders that this is acceptable — internal writes are not user-rejectable, so the pre-callback would have no effect anyway.

4. **`gEmberAttributeIOBufferSpan` concurrency**: this is a global mutable buffer. If any path becomes re-entrant (e.g., a callback triggers a read which uses the same buffer), corruption occurs. The depth guard in §4 prevents re-entrant write notifications, but this should be reviewed.
