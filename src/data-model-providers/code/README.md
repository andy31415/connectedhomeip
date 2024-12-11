---
orphan: true
---

## Contents

This provider is intended to allow building of data model providers without
requiring any generated code. Generally functionality for this is created using
`AttributeAccessInterface` and `CommandHandlerInterface`.

The `CodeDataModelProvider` class allows to combine cluster building blocks of:

- `AttributeAccessInterface`
- `CommandHandlerInterface`
- `ClusterMeta` as metadata

### Status

This is currently a **WORK IN PROGRESS**, known potential issues to be fixed:

- `CommandHandlerInterface` currently allows for `EnumerateAcceptedCommands` and
  `EnumerateGeneratedCommands` which is not currently friendly to first/next
  functionality for `DataModel::Provider` iteration

  - we may consider deprecating this style of enumeration (as even existing
      implementations generally have a `switch` equivalent for iteration)

  - we may switch `DataModel::Provider` to use this style of iteration for
      commands (however this is inconvenient for getting command metadata) open
      to other resolutions here

- Manually generating Metadata seems tedious and does not make use of existing
  tooling that has some built-in validations. We may want to at least be able to
  generate this data from existing data model XML files.
