#!/bin/bash
set -euo pipefail

# Check if the default server binary exists
if [ ! -x "./bin/webserv" ]; then
    echo "❌ Error: ./bin/webserv not found or not executable."
    exit 1
fi

# Directory of this script (even if called from elsewhere)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Export UBSAN options
export UBSAN_OPTIONS="suppressions=${SCRIPT_DIR}/.asanignore:print_stacktrace=1:halt_on_error=1:exitcode=77"

# Run webserv
exec "${SCRIPT_DIR}/bin/webserv" "$@"
