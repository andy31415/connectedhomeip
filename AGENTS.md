# AI Agent Guidelines for Matter SDK

This file provides guidelines and instructions for AI agents working on the
Matter SDK codebase.

## General Principles

-   **When in Rome**: Match the prevailing style of the code being modified. See
    [docs/style/CODING_STYLE_GUIDE.md](docs/style/CODING_STYLE_GUIDE.md).
-   **Atomicity**: Make small, incremental changes. Do not mix refactoring with
    feature implementation.
-   **No Filler Names**: Avoid names like "support", "common", "helpers",
    "util", "core". Use concrete names.
-   **Error Handling**: Use `CHIP_ERROR` as the standard return type for fallible operations.
    Prefer `VerifyOrReturnError` and `ReturnErrorOnFailure` macros for concise error checking and propagation.
-   **Logging**: Use the `ChipLog*` macros (e.g., `ChipLogProgress`, `ChipLogError`, `ChipLogDetail`) for logging.
    Ensure logs are appropriately categorized by module (e.g., `AppServer`, `InteractionModel`).

## Ignored Directories

When searching for files or code patterns, ignore the following directories
unless explicitly asked to look there:

-   `third_party/` (contains external dependencies)
-   `out/` (contains build artifacts)

## Code Review Instructions

-   Do not comment on content for XML files or `.matter` content for clusters.
-   The SDK implements an in-progress Matter specification that is private and under development.
    Assume the Matter specification is unknown and out of scope _unless_ you have explicit access
    to the latest version (e.g., via a specialized tool or skill).
-   Avoid "pat on the back" style comments that just restate what the code is doing.
    Focus on suggesting concrete code improvements.
-   Be concise. Do not over-explain code.
-   Look for common typos and suggest fixes.
-   Do not comment on whitespace or formatting (auto-formatters handle this).
-   Review changes for embedded development:
    -   Minimize use of heap allocation.
    -   Optimize for resource usage (RAM/Flash).
    -   Be cautious with complex templates that could lead to code bloat.
    -   Ensure proper error propagation (don't ignore `CHIP_ERROR` results).

## Coding Style (Highlights)

Refer to [docs/style/CODING_STYLE_GUIDE.md](docs/style/CODING_STYLE_GUIDE.md)
for full details.

-   **C++**: C++17 standard.
    -   Use `cstdint` for POD types.
    -   Avoid top-level `using namespace` in headers.
    -   Use anonymous namespaces for file-internal classes/objects.
    -   Avoid heap allocation and auto-resizing containers in core SDK.
-   **Python**: Python 3.11 standard.
    -   Use type hints on public APIs.
    -   Include docstrings for public APIs.

## Testing

-   Unit testing is _highly encouraged_. Unless it is impossible to unit test (e.g., platform-specific code),
    the codebase requires unit tests for all changes.
-   Tests in `src/python_testing` and `src/app/tests/suites` which verify
    expected failures should clearly indicate why the failure is expected.
    Include a summary of the relevant specification requirements if possible.

## Architectural Constraints

-   **Code-Driven Clusters**: When developing code-driven clusters, do not use
    any `ember` code or generated code from `CodegenIntegration` or `emberAf*`
    functions. Code-driven clusters must not depend on the Ember framework.
    Avoid types like `EmberAfStatus` or functions like `emberAfSendImmediateDefaultResponse`.

## Codebase Navigation

The Matter SDK is large. Use these tools effectively:
-   **ripgrep (rg)**: For fast searching within file contents.
    Example: `rg "symbol_name" src/app`
-   **fd**: For finding files by name.
    Example: `fd -e h "Discovery"`

## Common Commands

Most commands require an activated environment.

### Environment Activation
You can run commands within the environment using `scripts/run_in_build_env.sh`:
`scripts/run_in_build_env.sh "command"`

Alternatively, you can activate the environment in your shell:
`source scripts/activate.sh`

### Build and Test
-   **List available targets**:
    `scripts/run_in_build_env.sh "./scripts/build/build_examples.py targets"`
-   **Generate Ninja files**:
    `scripts/run_in_build_env.sh "./scripts/build/build_examples.py --target linux-x64-tests-clang --quiet gen"`
-   **Build and run all tests**:
    `scripts/run_in_build_env.sh "./scripts/build/build_examples.py --target linux-x64-tests-clang --quiet build"`
-   **Run a specific test**:
    `scripts/run_in_build_env.sh "ninja -C out/linux-x64-tests-clang --quiet path/to/test:test_name._run"`

### Building Common Apps
-   **chip-tool** (Interactive commissioning tool):
    `scripts/run_in_build_env.sh "./scripts/build/build_examples.py --target linux-x64-chip-tool-clang --quiet build"`
-   **all-clusters-app** (Feature-rich device simulator):
    `scripts/run_in_build_env.sh "./scripts/build/build_examples.py --target linux-x64-all-clusters-clang --quiet build"`
-   **all-devices-app** (Alternative feature-rich simulator):
    `scripts/run_in_build_env.sh "./scripts/build/build_examples.py --target linux-x64-all-devices-clang --quiet build"`

## Development Resources

-   [docs/guides/writing_clusters.md](docs/guides/writing_clusters.md)
-   [docs/guides/migrating_ember_cluster_to_code_driven.md](docs/guides/migrating_ember_cluster_to_code_driven.md)
-   [docs/testing/unit_testing.md](docs/testing/unit_testing.md)
-   [docs/testing/integration_tests.md](docs/testing/integration_tests.md)
