---
name: matter-specification-access
description: >
    Guidelines for accessing and reading the Matter specification and test plans.
    These resources are private and in-progress, so they must be cloned or requested.
---

# Matter Specification Access

## Overview

The Matter specification and test plans are private repositories. Agents cannot assume they know the latest specification content as it is actively developed. This skill describes how to obtain and read these documents.

## Repositories

-   **Matter Specification**: `git@github.com:CHIP-Specifications/connectedhomeip-spec.git`
-   **Test Plans**: `git@github.com:CHIP-Specifications/chip-test-plans.git`

## Cloning Guidelines

-   **Do NOT dirty the source tree**: Always clone into a location that is ignored or temporary.
    -   Recommended location: `out/` directory (e.g., `out/spec`, `out/test_plans`) as it is automatically ignored by the SDK build system and typically gitignored.
    -   Alternative: A system temporary directory.
-   **Use Shallow Cloning**: The repositories are very large. Always use `--depth 1` when cloning to save time and disk space.
    ```bash
    git clone --depth 1 git@github.com:CHIP-Specifications/connectedhomeip-spec.git out/spec
    git clone --depth 1 git@github.com:CHIP-Specifications/chip-test-plans.git out/test_plans
    ```

## Reading the Specification

The specification is written in **Asciidoc** format.

-   **Context Pollution**: Asciidoc files may contain extensive license blurbs that can pollute the LLM context.
-   **Conversion to Markdown**: It is highly recommended to convert the spec to Markdown for better readability and reduced noise.
    -   Use `make markdown-all` in the spec repository root to build everything (spec, application clusters, device library).
    -   Alternatively, use the tool `tools/matter-to-markdown.sh` (described in the spec project README).
-   **Targeted Reading**: The specification is extremely long. Avoid reading whole files if possible. Use `grep` or similar tools to extract only the relevant pieces of information (e.g., a specific cluster or device data).

## Reading Test Plans

Test plans are also in **Asciidoc** format.

-   **No Markdown Conversion**: There is currently no official markdown conversion flow for test plans. They should be read as Asciidoc.
-   **Location**: Individual cluster test plans are generally located in `src/cluster` within the test plans repository.
