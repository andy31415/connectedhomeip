# Backward Compatibility: Matter Attribute Change Callbacks

## Summary

The Matter SDK is actively migrating cluster implementations from the legacy Ember attribute table to "code-driven" clusters (`ServerClusterInterface`). This migration is the right architectural direction, but it silently breaks a large class of existing applications: apps that react to attribute changes via `MatterPreAttributeChangeCallback` / `MatterPostAttributeChangeCallback` stop receiving those notifications the moment a cluster they depend on is migrated. The breakage produces no compile error, no runtime warning, and no incorrect return value — the hardware simply stops responding.

The goal is to protect developers upgrading the SDK from this class of silent failure. At minimum, a broken callback should produce a visible compile-time signal. Ideally, existing application code should continue to work without modification. This document analyses the technical constraints and proposes a solution path.

---

## Presentation Slides

---

### Slide 1 — The Problem: Silent Breakage on SDK Upgrade

- The SDK is moving clusters from Ember → code-driven (`ServerClusterInterface`)
- Existing apps react to attribute changes via `MatterPostAttributeChangeCallback`
- Code-driven clusters bypass the Ember path entirely
- **Result: callbacks silently stop firing — no error, no warning, hardware stops responding**
- Already happening: `TemperatureControl`, `Identify` are code-driven; in-tree example apps still register handlers for them that are never called

---

### Slide 2 — What Developers Expect

```cpp
void MatterPostAttributeChangeCallback(..., ClusterId cluster, AttributeId attr, uint8_t * value)
{
    if (cluster == TemperatureControl::Id)
        MyDevice::SetTargetTemp(*reinterpret_cast<int16_t*>(value));  // never reached
}
```

- This code compiles, links, and runs — it just does nothing
- The attribute value IS updated correctly in the cluster; the side-effect is lost
- Developers upgrading SDK versions have no signal that anything changed

---

### Slide 3 — Goals

**Minimum (must have)**
- Make the failure visible: a compile-time error or deprecation warning when a callback is registered for a cluster that is now code-driven

**Target (should have)**
- "Just works": apps upgrading the SDK continue to receive callbacks without code changes
- Scope the compat cost to apps already using Ember — no flash/RAM cost for apps that don't

**Long-term (want)**
- Provide a typed, per-cluster alternative to the global switch-loop pattern
- Deprecate `MatterPre/PostAttributeChangeCallback` on a defined timeline

---

### Slide 4 — Why This Is Not Trivial

- The legacy callbacks expect `uint8_t*` in Ember's memory layout (native-endian integers, Pascal strings)
- Code-driven clusters receive writes as TLV and store values as typed C++ members
- **Producing the legacy format requires Ember attribute metadata** (`EmberAfAttributeType`, size, nullable flag)
- A generic solution at the `DataModel::Provider` level would need a standalone type registry — duplicating Ember's generated data and adding flash cost unconditionally
- Post-change: reading the value back via `AttributeValueEncoder` is not viable (it is a full network serialization object, not an in-process value reader)

---

### Slide 5 — Proposed Solution: Compatibility Scoped to `CodegenDataModelProvider`

**Key insight**: `CodegenDataModelProvider` already has the full `EmberAfAttributeMetadata` tables for every attribute — including code-driven ones. No separate registry is needed.

Two-part design:

1. **Generic `AttributeChangeListener` interface** — carries only the `ConcreteAttributePath`, no value, no Ember types, no TLV

2. **`CodegenDataModelProvider` implements the listener** — entirely self-contained, zero cluster changes required:
   - Pre-change: copies `TLVReader` (cheap), uses existing `EmberAfAttributeMetadata` to decode to Ember bytes, calls `MatterPreAttributeChangeCallback`
   - Post-change: calls cluster's existing `ReadAttribute` with a new `StackTLVEncoder` (bare TLVWriter over a ~16-byte stack buffer, no IM framing), decodes resulting TLV to Ember bytes, calls `MatterPostAttributeChangeCallback`
   - Apps not using `CodegenDataModelProvider` pay zero cost
   - Type safety comes from TLV — the same encoding the cluster already produces for reads

---

### Slide 6 — Open Issues for Discussion

- **Re-entrancy**: if a post-callback writes another attribute, do we recurse? Depth guard (silent drop), prohibition (assertion), or deferred queue (ordering changes)?
- **"Value changed" tracking**: should post-callback fire on no-op writes, or must the cluster signal that the value actually changed?
- **Deprecation commitment**: without a concrete removal milestone for the global callbacks, the shim becomes permanent infrastructure — is that acceptable?

---

## 1. The Problem

Historically, all attribute writes flowed through `emAfWriteAttribute` in `src/app/util/attribute-table.cpp`. This centralized path was the natural hook for two application-level callbacks:

1. **`MatterPreAttributeChangeCallback`** — called before a write. The application can reject it (return non-`Success`) or prepare hardware.
2. **`MatterPostAttributeChangeCallback`** — called after a write. The application reacts: drives a relay, updates dependent state, etc.

Code-driven clusters (`ServerClusterInterface` in `src/app/server-cluster/ServerClusterInterface.h`) bypass `attribute-table.cpp` entirely. They store state as typed C++ members and handle writes internally. As a result, **these callbacks are never invoked for code-driven clusters**, causing silent failures: the attribute value changes correctly, but application-side effects do not happen.

### Known Breakage

`examples/refrigerator-app/silabs/src/DataModelCallbacks.cpp` registers post-change handlers for `TemperatureControl` and `Identify` — both of which are already code-driven. Those handlers are never called.

---

## 2. Callback Signatures

```cpp
chip::Protocols::InteractionModel::Status MatterPreAttributeChangeCallback(
    const chip::app::ConcreteAttributePath & attributePath,
    uint8_t type,    // EmberAfAttributeType enum value
    uint16_t size,
    uint8_t * value);

void MatterPostAttributeChangeCallback(
    const chip::app::ConcreteAttributePath & attributePath,
    uint8_t type,
    uint16_t size,
    uint8_t * value);
```

`value` points to a byte buffer in Ember's layout: native-endian integers, length-prefixed (Pascal) strings. Existing application code commonly casts it directly to `bool*` or `uint8_t*`.

---

## 3. Challenges

### A. Value Access on the Write Path (Pre-Change)

For the pre-change callback, the incoming value is carried as a positioned `TLVReader` inside an `AttributeValueDecoder` (`src/app/AttributeValueDecoder.h`). The decoder itself is lightweight — it is a thin wrapper around a `TLVReader&` reference with no significant state.

The key constraint is that `TLVReader` is a forward-only cursor: once advanced (by calling `Decode()`), it cannot be rewound by the consumer. The cluster needs the same reader to decode its own write.

Crucially, `TLVReader` is a copyable value type. A copy of the reader made before forwarding to the cluster gives the shim an independent cursor positioned at the same data: copy the reader, decode to Ember bytes using `EmberAttributeDataBuffer`, call the pre-callback, then let the cluster use the original reader.

For scalars, the decode cost is a few bytes read from the TLV cursor. For strings, the decode produces the full Pascal string into the Ember buffer — the cost is proportional to the string length and requires a buffer large enough for the attribute's maximum string size (sourced from `EmberAfAttributeMetadata`). This is unavoidable regardless of approach; the legacy callback must receive the full string bytes.

### B. Value Access on the Write Path (Post-Change)

For the post-change callback, the cluster has already committed the value to its typed C++ members. Options:

1. **Read-back through the read path**: Invoke the cluster's `ReadAttribute`, which produces an `AttributeValueEncoder`. This is not viable — `AttributeValueEncoder` (`src/app/AttributeValueEncoder.h`) requires a live `AttributeReportIBs::Builder`, a `SubjectDescriptor`, chunked list state, and a full TLV write buffer. It is designed for network serialization, not for in-process value inspection. Constructing this infrastructure just to extract a byte value and immediately discard it would be expensive in both code size and stack usage.

2. **Cluster pushes the new value via a lightweight notification**: The cluster, at the moment of committing a value, calls a simple listener with the new value as raw bytes. For integer scalars, the C++ member value IS already in Ember format (native-endian), so the "conversion" is a pointer cast with no computation. For strings, the cluster provides a `ByteSpan`/`CharSpan`; the shim prepends the Pascal length prefix into a small stack buffer. This covers the vast majority of real callback usage.

Option 2 is the correct approach. It means the cluster — not the shim — is responsible for providing the value at the moment of change. This is always accurate and requires no external state.

### C. Value Representation in the Notification Interface

In practice, the attribute types that actually appear in these callbacks are scalars (bool, int8–int64, uint8–uint64, enums, bitmaps) and strings (char/octet). Ember never passed full list or struct content through `MatterPostAttributeChangeCallback`; those cases can be excluded from the notification interface.

Three options:

**Option A — Path-only notification, read-back via a lightweight encoder**

The generic interface carries only the `ConcreteAttributePath`. After a write, `CodegenDataModelProvider` reads the new value back using the cluster's existing `ReadAttribute` with a `StackTLVEncoder` — a stripped-down encoder backed by a bare `TLVWriter` over a local buffer, with no IM framing.

The buffer sizing is the problem: for scalars, TLV needs 5–9 bytes (fixed, safe on the stack). For strings, TLV needs the full string length plus a few bytes of overhead, which can vary up to the attribute's maximum string size. That maximum is known from `EmberAfAttributeMetadata`, but a correct implementation either allocates from the heap (undesirable on embedded targets) or sizes the buffer at compile time using the largest possible string attribute — which may waste significant stack space on every write, not just writes to string attributes.

Type-safety comes from TLV. Zero changes to `ServerClusterInterface` or any cluster implementation. The `StackTLVEncoder` is a new but small and independently useful primitive.

**Option B — TLV value carried in the notification**

The generic interface carries a `TLVReader&` positioned at the committed value. The cluster encodes its value into a stack buffer and passes a reader. Type-safe; atomic with the notification. Same string buffer sizing problem as Option A — the cluster must allocate a buffer large enough for its string value, but it already has the string as a `CharSpan` whose backing storage is available at write time, so no additional copy is needed.

Requires the cluster to write notification encoding code at each commit point. More boilerplate, but the cluster controls the buffer lifetime precisely.

**Option C — Tagged raw-pointer descriptor**

The notification interface carries a small struct: a raw data pointer, the data size, and an `isString` flag:

```cpp
struct AttributeRawValue {
    const void * data;
    size_t       size;
    bool         isString;
};
```

For scalars: `data = &myMember, size = sizeof(myMember), isString = false`. The C++ member is already in native-endian format, which is Ember's expected layout — no encoding step.

For strings: `data = span.data(), size = span.size(), isString = true`. The cluster points directly into its existing string storage; no copy or buffer management required on the cluster side.

`CodegenDataModelProvider`, which has `EmberAfAttributeMetadata` for every attribute, can validate the provided descriptor: for scalars, verify that `size` exactly matches the metadata's expected byte width (e.g., reject 4 bytes for an `int16_t` attribute). For strings, verify `size <= metadata.maxLength`. The shim then converts to Ember format: scalars are a `memcpy`; strings prepend the Pascal length byte into a buffer sized from `metadata.maxLength + 1`.

This is not equivalent to Ember's unvalidated `uint8_t*`. The struct carries enough information for the provider to detect common mistakes at runtime, and the provider enforces the contract using its own metadata. It is less formally type-safe than TLV (no encode/decode round-trip to catch type mismatches), but in practice the attribute types involved are scalar or string, and those can be validated structurally.

The main downside is that the `isString` flag and raw pointer are a weak typing mechanism. A scalar passed with the wrong `size` would be caught; a scalar of the right size but wrong sign/type (e.g., `uint16_t` vs `int16_t`) would not. Whether that residual risk is acceptable depends on how much the cluster author is trusted to pass the right member.

**Recommendation**: Option C is the most efficient in practice — no encoding at all for scalars, no buffer management for strings, and provider-side validation catches the most likely mistakes. Option A is the cleanest architecturally (zero cluster API changes, TLV type safety) but its stack buffer strategy requires care for string attributes. Option B has the same buffer issue as A with added cluster boilerplate. The choice between A and C should be made with the string buffer strategy resolved first; if large string attributes are common among clusters that need these callbacks, Option C's avoidance of intermediate buffers is a material advantage.

### D. Flash / Code Size Cost

Any approach operating at the generic `DataModel::Provider` level would require a standalone `(ClusterId, AttributeId)` → `EmberAfAttributeType` registry, duplicating what the Ember code-generation already provides. On constrained targets (Cortex-M0/M4, 256–512 kB flash), this is unacceptable overhead for apps that do not use these callbacks.

Scoping the compatibility layer to `CodegenDataModelProvider` avoids this: `EmberAfAttributeMetadata` is already present in the generated tables for every attribute (including code-driven ones). The additional cost is limited to the conversion routines and notification dispatch, which can be conditionally compiled.

### E. Injection Points and Notification Timing

- **Pre-change**: `CodegenDataModelProvider::WriteAttribute` is the injection point. It controls dispatch, has access to the TLVReader (via the decoder), and has the Ember metadata. TLV reader copy (Challenge A) makes this feasible.
- **Post-change (reportable changes)**: `ProviderChangeListener::MarkDirty()` is only called for changes that trigger subscription reporting. Ember called the post-callback for every value change regardless of reporting. Tying the post-callback to `MarkDirty` would silently drop notifications for "quiet" writes.
- **Post-change (all changes)**: The cluster's lightweight notification (Challenge B/C) decouples the two concerns. `MarkDirty` continues to drive reporting; the notification interface drives the application callback.

### F. Recursive Callbacks

A post-callback may write to another attribute (e.g., syncing a derived value). If that write re-triggers the callback machinery, the result is recursive invocation — a stack overflow risk on targets with shallow stacks (4–8 kB).

Three options:

1. **Depth guard**: A re-entrancy counter blocks callbacks beyond depth 1. Simple, but silently drops inner notifications.
2. **Prohibit attribute writes from callbacks**: Enforce via assertion. Semantically clean, but may break existing patterns.
3. **Deferred queue**: Notifications triggered during a callback are queued and dispatched after the current callback returns. Preserves all notifications and avoids re-entrancy, but changes observable ordering relative to legacy Ember (which was synchronous and implicitly recursive).

---

## 4. Approaches

### Approach 1: Shim in the Generic Provider Routing Layer

The base `DataModel::Provider` dispatch intercepts every write, peeks TLV, converts to Ember bytes using a standalone type registry, calls `MatterPreAttributeChangeCallback`, forwards to the cluster, and on success calls `MatterPostAttributeChangeCallback`.

- **Pro**: Centralized — no changes needed in cluster implementations.
- **Con**: Requires a standalone type registry duplicating Ember's generated data (Challenge D). Hard to scope away from non-Ember targets. The cluster must signal "value actually changed" back to the provider layer.

### Approach 2: Per-Cluster Helper

Each `ServerClusterInterface` implementation calls a shared helper before and after committing a write. The helper serializes the value to Ember format and calls the global callbacks.

- **Pro**: Cluster knows the exact type and change status — no TLV peeking needed.
- **Con**: Every cluster must call the helper; omissions are silent bugs. Couples all cluster code to Ember metadata. Flash cost is paid per-cluster.

### Approach 3: Typed Hooks on `ProviderChangeListener`

Add pre/post notification methods to `src/app/data-model-provider/ProviderChangeListener.h`, decoupled from `MarkDirty`. A concrete listener bridges to the legacy global callbacks.

- **Pro**: Fits the dependency-injection pattern; re-entrancy policy is encapsulated in one place.
- **Con**: Same per-cluster adoption burden as Approach 2. Unless scoped to `CodegenDataModelProvider`, the standalone type registry problem (Challenge D) is not avoided.

### Approach 4: Compatibility Layer Scoped to `CodegenDataModelProvider` (Recommended)

Split into two parts:

**Part 1 — Generic notification interface**:

The notification interface carries the path and a value descriptor. The exact value representation is unresolved (see Challenge C); two viable shapes are:

```cpp
// Option A/B shape — path-only; provider reads back via StackTLVEncoder
class AttributeChangeListener {
    virtual Status OnPreAttributeChange(const ConcreteAttributePath &) = 0;
    virtual void   OnPostAttributeChange(const ConcreteAttributePath &) = 0;
};

// Option C shape — tagged raw descriptor; cluster provides pointer + size + kind
struct AttributeRawValue { const void * data; size_t size; bool isString; };
class AttributeChangeListener {
    virtual Status OnPreAttributeChange(const ConcreteAttributePath &, AttributeRawValue) = 0;
    virtual void   OnPostAttributeChange(const ConcreteAttributePath &, AttributeRawValue) = 0;
};
```

For pre-change, the value is always available from the incoming `TLVReader` regardless of which shape is chosen — the provider handles this without cluster involvement. The shape choice primarily affects the post-change path.

**Part 2 — Ember compatibility implementation in `CodegenDataModelProvider`**:

- **Pre-change**: copies the incoming `TLVReader`, decodes to Ember bytes via `EmberAttributeDataBuffer` + existing `EmberAfAttributeMetadata`, calls `MatterPreAttributeChangeCallback`. If rejected, cluster is not invoked. No cluster changes needed regardless of value shape.
- **Post-change (Option A)**: calls the cluster's existing `ReadAttribute` with a `StackTLVEncoder`. Decodes resulting TLV to Ember bytes. No cluster API changes. String attribute buffer sizing requires care (see Challenge C).
- **Post-change (Option C)**: receives `AttributeRawValue` from the cluster. Validates `size` against `EmberAfAttributeMetadata`. For scalars: direct copy. For strings: prepend Pascal length into a buffer sized from `metadata.maxLength`. Requires cluster to pass its value descriptor at commit time.

```
DataModel::Provider
  └── AttributeChangeListener (no Ember types)
        └── CodegenDataModelProvider implements:
              pre:  TLV reader copy → EmberAttributeDataBuffer → MatterPreAttributeChangeCallback
              post: StackTLVEncoder (Option A) or AttributeRawValue validation (Option C)
                    → EmberAttributeDataBuffer → MatterPostAttributeChangeCallback
```

- **Pro**: No standalone type registry. Non-Ember apps pay no cost. Re-entrancy managed centrally. Both options keep Ember-specific logic entirely within `CodegenDataModelProvider`.
- **Con**: Option A requires resolving string buffer sizing; Option C requires clusters to call the listener at commit time (omissions are silent). The unresolved choice between A and C is the main open item for this approach.

---

## 5. Deprecation Path

The global `MatterPre/PostAttributeChangeCallback` with application-wide switch-case loops has significant structural problems:

- **Not type-safe**: Values arrive as `uint8_t*` with a type tag; incorrect casts are silent.
- **Monolithic entry point**: All clusters share one function. As the application grows, the switch loop grows. Other SDKs and platforms have adopted this pattern because no alternative existed, making it harder to remove.
- **Permanent crutch risk**: If the compatibility shim makes these callbacks work transparently for code-driven clusters, there is no pressure to migrate. The pattern becomes entrenched indefinitely.

The natural migration target already exists: code-driven clusters are C++ classes. Application-specific logic that currently lives in `MatterPostAttributeChangeCallback` belongs as a virtual method override on the cluster subclass, or as a per-cluster delegate injected at construction time. This is already how the rest of the Data Model Provider architecture works.

Proposed deprecation strategy:

1. **Implement Approach 4** (Ember-scoped compatibility shim). Mark `MatterPreAttributeChangeCallback` and `MatterPostAttributeChangeCallback` with a compile-time deprecation notice when used with `CodegenDataModelProvider`.
2. **Introduce `AttributeChangeListener`** as the documented forward path. Provide an example of replacing a switch-case callback with a cluster subclass or per-cluster delegate.
3. **Define a removal milestone** (e.g., next major SDK version). Without a concrete date, the deprecated API will persist indefinitely.

The generic `AttributeChangeListener` interface (Part 1 of Approach 4) is the long-term replacement. It is typed, per-cluster, injectable, testable, and carries no Ember dependency. The compatibility shim in `CodegenDataModelProvider` is explicitly a bridge, not a permanent API.

---

## 6. Open Questions

1. **Post-change value representation (Challenge C)**: Option A (path-only + `StackTLVEncoder` read-back) requires no cluster changes and is type-safe via TLV, but the stack buffer strategy for string attributes needs resolution — allocating per-write at the attribute's max string size may be costly. Option C (`AttributeRawValue` descriptor with provider-side validation) is more efficient but requires clusters to call the listener at commit time. Which trade-off is acceptable? The answer likely depends on how many code-driven clusters have large string attributes that need these callbacks.

2. **Pre-change rejection semantics**: Should a rejection from `MatterPreAttributeChangeCallback` map to a specific IM status, or always `UNSUPPORTED_WRITE`? The legacy Ember behavior returned the callback's return value directly.

3. **"Value actually changed" signal (Challenge E)**: The post-callback was traditionally only invoked when the value changed (not on a no-op write). Should the notification interface include a "previous value" for comparison, or require clusters to track this themselves before calling the listener?

4. **Re-entrancy policy (Challenge F)**: Which strategy (depth guard, prohibition, deferred queue) matches what existing application callbacks actually do? A survey of in-tree example apps would answer this.

5. **Deprecation timeline**: What is the committed removal milestone for `MatterPre/PostAttributeChangeCallback`? Without one, the compatibility shim becomes permanent infrastructure.
