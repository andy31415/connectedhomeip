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
RUN_COVERAGE=false
RUN_CONFORMANCE=false
DEPLOY=false
SERVE=false
OUT_DIR="out/coverage/coverage/html"
TEMPLATE="integrations/compute_engine/report_not_generated.html.template"

help() {
    echo "Usage: $0 [options]"
    echo
    echo "Options:"
    echo "  --coverage        Generate coverage report"
    echo "  --conformance     Generate conformance report"
    echo "  --all             Generate all reports"
    echo "  --deploy          Deploy to App Engine"
    echo "  --serve, --test   Start local server to view reports"
    echo "  -h, --help        Print this help"
    echo
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --coverage)
            RUN_COVERAGE=true
            shift
            ;;
        --conformance)
            RUN_CONFORMANCE=true
            shift
            ;;
        --all)
            RUN_COVERAGE=true
            RUN_CONFORMANCE=true
            shift
            ;;
        --deploy)
            DEPLOY=true
            shift
            ;;
        --serve|--test)
            SERVE=true
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

# Ensure output directory exists
mkdir -p "$OUT_DIR"

write_dummy() {
    local file=$1
    local name=$2
    echo "Generating dummy page for $name..."
    sed "s/__REPORT_NAME__/$name/g" "$TEMPLATE" > "$OUT_DIR/$file"
}

# --- Coverage Report ---
if [ "$RUN_COVERAGE" = true ]; then
    echo "Generating Coverage Report..."
    ./scripts/build_coverage.sh
else
    write_dummy "index.html" "Coverage Report"
fi

# --- Conformance Report ---
if [ "$RUN_CONFORMANCE" = true ]; then
    echo "Generating Conformance Report..."
    source scripts/activate.sh
    ./scripts/build_python.sh -w false -i out/python_env
    python3 -u scripts/examples/conformance_report.py
    cp /tmp/conformance_report/conformance_report.html "$OUT_DIR/"
else
    write_dummy "conformance_report.html" "Conformance Report"
fi

# --- Deploy ---
if [ "$DEPLOY" = true ]; then
    echo "Deploying to App Engine..."
    (
        cd out/coverage/coverage
        gcloud app deploy webapp_config.yaml --quiet

        # Cleanup old versions, keep last 5
        versions=$(gcloud app versions list \
            --service default \
            --sort-by '~VERSION.ID' \
            --format 'value(VERSION.ID)' | sed 1,5d)
        
        for version in $versions; do
            gcloud app versions delete "$version" \
                --service default \
                --quiet
        done
    )
fi

# --- Serve ---
if [ "$SERVE" = true ]; then
    echo "Starting local server at http://localhost:8000"
    (
        cd "$OUT_DIR"
        python3 -m http.server 8000
    )
fi
