## Example input file

```yaml
versions:
    java: "17.0"
    kotlin: "11.22"
data:
    # This format is fuzzy, however unique given matter idl generation rules
    OnOff: [new]
    BasicInformation.productAppearance: [new]        # notice lowerCamelCase for attribute
    BasicInformation.ProductAppearanceStruct: [new]  # notice UpperCamelCase for struct
    BasicInformation.ColorEnum: [provisional]
    Identify.IdentifyTypeEnum.kLightOutput: [new, alias=kVisibleLight]
    FanControl.FanModeEnum: [new, alias=FanModeType]
    "*.eventList": [hidden]                          # key must be escaped in this case

    # The same as above, in a never-ambiguous format
    OnOff: [new]
    BasicInformation.productAppearance/attribute: [new]
    BasicInformation.ProductAppearanceStruct/struct: [new]
    BasicInformation.ColorEnum/enum: [provisional]
    Identify.IdentifyTypeEnum/enum.kLightOutput: [new, alias=kVisibleLight]
    FanControl.FanModeEnum/enum: [new, alias=FanModeType]
    "*.eventList": [hidden]
```

### Key formats

| Key                                 | Unambiguous        | Description                    | Example                          |
|-------------------------------------|--------------------|--------------------------------|----------------------------------|
| `[cluster]`                         | :heavy_check_mark: | specifies a cluster            | `BasicInformation`               |
| `[cluster].[member]`                | :heavy_minus_sign: | a member within a cluster      | `OnOff.Feature`                  |
| `[cluster].[member]/[type]`         | :heavy_check_mark: | a member with known type       | `OnOff.Feature/enum`             |
| `[cluster].[member].[field]`        | :heavy_minus_sign: | a field with a cluster member  | `LevelControl.MoveMode.kUp`      |
| `[cluster].[member]/[type].[field]` | :heavy_check_mark: | a field with a cluster member  | `LevelControl.MoveMode/enum.kUp` |
| `*.[name]`                          | :heavy_check_mark: | represents a global attribute  | `*.eventList`                    |

- **type** in the above format must be one of:
  - **attribute**
  - **bitmap**
  - **command**
  - **enum**
  - **event**
  - **struct**

- **fields** may be:
  - constant names for **bitmap** or **enum**
  - field member names for **struct** (including request and response structures for commands) or *event*


## Release file format details

Release files are `yaml` files with the following top level keys:


- **versions** contains information about what API metadata is contained in the file:

  - the key relates to some API version (e.g. "ios", "kotlin", "java", ...)
  - the value represents the actual release version (e.g. "1.0", "1.2.3", ...)
  - multiple version entries may exist for APIs that get released in parallel

- **data** contains versioning version information where:

  - the key represents the element to which the metadata applies.
  - the value is a _list_ containing various metadata entries for this particular key, specifically

      - **new** marks this key as introduced/new in this release
      - **hide** marks this key as not to be generated as part of API generation
      - **provisional** marks this key as temporary/provisional for this API generation
      - **alias=&lt;name&gt;** marks that this particular key should be aliased with a separate
        name. This is typically to provide a backwards compatibility layer to for users of
        previous API releases


### Data key format

The general format is to use `"."` as a separator between elements within a hierarchy of elements
that are used in APIs, specifically `ClusterName > MemberName > FieldName`.

For type-specification to remove fuzzyness, a `"/"` is used as a separator between name and type.

The data key format describes both a fuzzy/potentially-ambiguous key naming scheme and a fully
expanded and unambiguous format. The ambiguity arises because member names within `ClusterName.MemberName` may share the same naming within the matter specification: for example `ThreadNetworkDiagnostics::SecurityPolicy` is both the name of a structure and the name of the attribute containing this structure.

In the current specification, ambiguity is restricted to only attribute names potentially
sharing the same name with data types and there are no occurrences where data types collide (like
the same name being shared between enumerations, bitmaps or structures). 

For future specification updates, it seems the norm to have a type suffix like `Enum`, `Bitmap` or `Struct` for most
cases (except `Feature` which has no suffix), so this should minimize name collisions if applied.

Given that ambiguity is only between attributes and data types, the expected release format matches
`.matter` code generation rules:

  - `UpperCamelCase` is used for:
    - bitmaps
    - commands
    - enumerations
    - events
    - structures

  - `lowerCamelCase` is used for:
    - attributes

Commands that have request/response structures declare them separately as `[CommandName]Request` and `[CommandName]Response`


