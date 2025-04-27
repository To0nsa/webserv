#!/bin/bash
# set -euo pipefail:
#   -e  : Exit immediately if any command exits with a non-zero status.
#   -u  : Treat unset variables as an error and exit immediately.
#   -o pipefail : Return the exit status of the last command in a pipeline that failed
set -euo pipefail

# Colors
GREEN="\033[0;32m"
RED="\033[0;31m"
CYAN="\033[0;36m"
YELLOW="\033[1;33m"
RESET="\033[0m"

# Path to suppression file
SUPPRESSION_FILE="$(dirname "$0")/.asanignore"

function run_tests() {
    local preset="$1"
    local build_dir="build/${preset}"

    echo -e "${CYAN}🔧 Preparing build for preset: ${preset}${RESET}"

    # Clean previous build if it exists
    if [ -d "$build_dir" ]; then
        echo -e "${YELLOW}🧹 Cleaning previous build directory: ${build_dir}${RESET}"
        rm -rf "$build_dir"
    fi

    echo -e "${CYAN}⚙️  Configuring with CMake preset: ${preset}${RESET}"
    cmake --preset "$preset" -DBUILD_TESTING=ON

    echo -e "${CYAN}🏗️  Building project with preset: ${preset}${RESET}"
    cmake --build --preset "$preset"

    echo -e "${CYAN}🧪 Running tests for preset: ${preset}${RESET}"

    # Set sanitizer suppression environment variables dynamically
    if [[ "$preset" == "asan" ]]; then
        export ASAN_OPTIONS="detect_leaks=1:suppressions=${SUPPRESSION_FILE}:exitcode=42:fast_unwind_on_malloc=0"
    elif [[ "$preset" == "tsan" ]]; then
        export TSAN_OPTIONS="suppressions=${SUPPRESSION_FILE}:exitcode=42"
    elif [[ "$preset" == "ubsan" ]]; then
        export UBSAN_OPTIONS="suppressions=${SUPPRESSION_FILE}:print_stacktrace=1:exitcode=42"
    else
        unset ASAN_OPTIONS TSAN_OPTIONS UBSAN_OPTIONS
    fi

    if ! ctest --preset "$preset" --output-on-failure; then
        echo -e "${RED}❌ Tests failed for preset: ${preset}${RESET}"
        exit 1
    fi

    echo -e "${GREEN}✅ Tests passed for preset: ${preset}${RESET}\n"
}

start_time=$(date +%s)

echo -e "${CYAN}🚀 Starting full test run for all configurations...${RESET}\n"

# List of presets to test
presets=("debug" "asan" "tsan" "ubsan")

for preset in "${presets[@]}"; do
    run_tests "$preset"
done

end_time=$(date +%s)
elapsed=$((end_time - start_time))

echo -e "${GREEN}🏆 All builds and tests completed successfully in ${elapsed} seconds!${RESET}"
