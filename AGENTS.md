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

## Ignored Directories

When searching for files or code patterns, ignore the following directories
unless explicitly asked to look there:

-   `third_party/` (contains external dependencies)
-   `out/` (contains build artifacts)

## Code Review Instructions

-   Do not comment on content for XML files or `.matter` content for clusters.
-   The SDK is implementing an in-progress Matter specification that has private
    visibility and in development. Assume the Matter specification is unknown
    and out of scope _unless_ you have explicit access to the latest version
    (access to a user supplied checkout, ability to access it via a skill or
    similar).
-   Avoid "pat on the back" style comments that just restate what the code is
    doing. Focus on review that suggests concrete improvements to the code.
    Suggest only explicit changes that will result in an improvement in code.
-   Be concise. Do not over-explain code.
-   Look for common typos and suggest fixes.
-   Do not comment on whitespace or formatting (auto-formatters handle this).
-   Review changes for embedded development: minimize use of heap, optimize for
    resource usage (e.g. be concerned about code size when code gets complex or
    too many template specializations could be instantiated).

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

-   Unit testing is _highly encouraged_. Unless impossible to unit test (e.g.
    platform specific code), codebase requires unit tests for any changes.
-   Tests in `src/python_testing` and `src/app/tests/suites` which verify
    expected failures should clearly indicate (either in the test name/label or
    in a failure output) why the failure is expected. For example, if there is a
    specific section in the spec which requires the failure, a summary of the
    section requirements should be included.

## Architectural Constraints

-   **Code-Driven Clusters**: When developing code-driven clusters, do not use
    any `ember` code or generated code from `CodegenIntegration` or `emberAf*`
    functions. Code-driven clusters must not depend on the Ember framework.

## Development Resources

-   [docs/guides/writing_clusters.md](docs/guides/writing_clusters.md)
-   [docs/guides/migrating_ember_cluster_to_code_driven.md](docs/guides/migrating_ember_cluster_to_code_driven.md)
-   [docs/testing/unit_testing.md](docs/testing/unit_testing.md)
-   [docs/testing/integration_tests.md](docs/testing/integration_tests.md)

## Common Commands

-   Most commands require an activated/bootstrapped environment. Usually this is
    done via `scripts/run_in_build_env.sh`
-   Example build commands:
    -   Generate the test directory (generates ninja files, but does not run):
        `scripts/run_in_build_env.sh "./scripts/build/build_examples.py --target linux-x64-tests-clang --quiet gen"`
    -   Generate _and_ run tests:
        `scripts/run_in_build_env.sh "./scripts/build/build_examples.py --target linux-x64-tests-clang --quiet build"`
    -   Run a single specific test (compiles and runs) once test directory
        exists:
        `scripts/run_in_build_env.sh "ninja -C out/linux-x64-tests-clang --quiet src/app/clusters/scenes-server/tests:TestScenesManagementCluster._run"`
