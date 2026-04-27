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
GENERATED_XML="out/generated_xml"
SDK_ROOT="."
SPEC_ROOT="out/spec"
SKIP_UPDATE=false

# Default versions/branches
ALCHEMY_VERSION="1.6.14"
SPEC_BRANCH="master"

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
    local branch=$4

    if [ "$SKIP_UPDATE" = true ] && [ -d "$dir" ]; then
        echo "Using existing $name directory as requested."
        return
    fi

    if [ -d "$dir" ]; then
        echo "Updating $name to $branch..."
        (
            cd "$dir"
            git fetch --depth 1 origin "$branch"
            git checkout FETCH_HEAD
        )
    else
        echo "Cloning $name ($branch)..."
        git clone --depth 1 --branch "$branch" "$url" "$dir"
    fi
}

# Clone/Update Spec (Private)
if [ -n "$GITHUB_PAT" ]; then
    update_or_clone "$SPEC_ROOT" "https://${GITHUB_PAT}@github.com/CHIP-Specifications/connectedhomeip-spec.git" "Spec" "$SPEC_BRANCH"
else
    update_or_clone "$SPEC_ROOT" "git@github.com:CHIP-Specifications/connectedhomeip-spec.git" "Spec" "$SPEC_BRANCH"
fi

# Download Alchemy Release
ALCHEMY_ASSET="alchemy-${ALCHEMY_VERSION}-linux-amd64.tar.gz"
ALCHEMY_URL="https://github.com/project-chip/alchemy/releases/download/v${ALCHEMY_VERSION}/${ALCHEMY_ASSET}"

if [ "$SKIP_UPDATE" = true ] && [ -f "out/alchemy" ]; then
    echo "Using existing Alchemy binary."
else
    echo "Downloading Alchemy v${ALCHEMY_VERSION}..."
    curl -L "$ALCHEMY_URL" -o "out/${ALCHEMY_ASSET}"
    
    echo "Extracting Alchemy..."
    # Extract to out/ directory
    tar -xzf "out/${ALCHEMY_ASSET}" -C out/
    
    chmod +x out/alchemy
fi

# Run Diff
echo "Running Alchemy Diff..."
mkdir -p "$GENERATED_XML"
./out/alchemy zap-diff \
    --xml-root-1 "$BASELINE_XML" \
    --xml-root-2 "$GENERATED_XML" \
    --out "$GENERATED_XML" \
    --format "both"

echo "Done. Check $GENERATED_XML for results."
