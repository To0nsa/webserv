#!/bin/bash
set -euo pipefail

# Check if the default server binary exists
if [ ! -x "./bin/webserv" ]; then
    echo "❌ Error: ./bin/webserv not found or not executable."
    exit 1
fi

# Directory of this script (even if called from elsewhere)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Export ASAN options
export ASAN_OPTIONS="detect_leaks=1:leak_check_at_exit=1:fast_unwind_on_malloc=0:abort_on_error=1:suppressions=${SCRIPT_DIR}/.asanignore"

# Run webserv
exec "${SCRIPT_DIR}/bin/webserv" "$@"
