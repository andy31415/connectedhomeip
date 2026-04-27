#!/usr/bin/env bash

#
# Copyright (c) 2026 Project CHIP Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

set -e

# Default values
USE_SECRET=""
BASELINE_XML="src/app/zap-templates/zcl/data-model/chip"
GENERATED_XML="out/alchemy/generated_xml"
SDK_ROOT="."
SPEC_ROOT="out/spec"
SKIP_UPDATE=false

help() {
    echo "Usage: $0 [options]"
    echo
    echo "Options:"
    echo "  --use-pat-secret NAME  Name of the gcloud secret containing the GitHub PAT"
    echo "  --baseline-xml PATH    Path to directory containing baseline ZAP XMLs (default: $BASELINE_XML)"
    echo "  --generated-xml PATH   Path to directory where new XMLs will be generated (default: $GENERATED_XML)"
    echo "  --spec-root PATH       Path to directory containing the spec repository (default: $SPEC_ROOT)"
    echo "  --skip-update          Skip updating existing checkouts (default: false)"
    echo "  -h, --help             Print this help"
    echo
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --use-pat-secret)
            USE_SECRET="$2"
            shift 2
            ;;
        --baseline-xml)
            BASELINE_XML="$2"
            shift 2
            ;;
        --generated-xml)
            GENERATED_XML="$2"
            shift 2
            ;;
        --spec-root)
            SPEC_ROOT="$2"
            shift 2
            ;;
        --skip-update)
            SKIP_UPDATE=true
            shift
            ;;
        -h|--help)
            help
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            help
            exit 1
            ;;
    esac
done

# Check Go
if ! command -v go &> /dev/null; then
    echo "Error: Go is not installed. Please install Go to build Alchemy."
    exit 1
fi

# Handle PAT
GITHUB_PAT=""
if [ -n "$USE_SECRET" ]; then
    echo "Fetching PAT from secret: $USE_SECRET"
    GITHUB_PAT=$(gcloud secrets versions access latest --secret="$USE_SECRET")
fi

# Ensure output directory exists
mkdir -p out

update_or_clone() {
    local dir=$1
    local url=$2
    local name=$3

    if [ "$SKIP_UPDATE" = true ] && [ -d "$dir" ]; then
        echo "Using existing $name directory as requested."
        return
    fi

    if [ -d "$dir" ]; then
        echo "Updating $name to latest version..."
        (
            cd "$dir"
            git fetch --depth 1
            git reset --hard origin/master 2>/dev/null || git reset --hard origin/main
        )
    else
        echo "Cloning $name..."
        git clone --depth 1 "$url" "$dir"
    fi
}

# Clone/Update Alchemy (Public)
update_or_clone "out/alchemy" "https://github.com/project-chip/alchemy.git" "Alchemy"

# Clone/Update Spec (Private)
if [ -n "$GITHUB_PAT" ]; then
    update_or_clone "$SPEC_ROOT" "https://${GITHUB_PAT}@github.com/CHIP-Specifications/connectedhomeip-spec.git" "Spec"
else
    update_or_clone "$SPEC_ROOT" "git@github.com:CHIP-Specifications/connectedhomeip-spec.git" "Spec"
fi

# Build Alchemy
echo "Building Alchemy..."
(
    cd out/alchemy
    GOOS=linux GOARCH=amd64 go build -o ../alchemy-action-linux-x64 -tags github
)

# Run Diff
echo "Running Alchemy Diff..."
mkdir -p "$GENERATED_XML"
./out/alchemy-action-linux-x64 zap-diff \
    --baseline-xml "$BASELINE_XML" \
    --generated-xml "$GENERATED_XML" \
    --sdk-root "$SDK_ROOT" \
    --spec-root "$SPEC_ROOT"

echo "Done. Check $GENERATED_XML for results."
