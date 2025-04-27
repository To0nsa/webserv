#!/bin/bash
set -euo pipefail

# Check if the default server binary exists
if [ ! -x "./bin/webserv" ]; then
    echo "❌ Error: ./bin/webserv not found or not executable."
    exit 1
fi

# If no arguments are provided, run the default server
if [ $# -eq 0 ]; then
    exec ./bin/webserv
fi

# Dispatch based on first argument
case "$1" in
    asan)
        if [ ! -x "./run_webserv_asan.sh" ]; then
            echo "❌ Error: ./run_webserv_asan.sh not found or not executable."
            exit 1
        fi
        shift
        exec ./run_webserv_asan.sh "$@"
        ;;
    tsan)
        if [ ! -x "./run_webserv_tsan.sh" ]; then
            echo "❌ Error: ./run_webserv_tsan.sh not found or not executable."
            exit 1
        fi
        shift
        exec ./run_webserv_tsan.sh "$@"
        ;;
    ubsan)
        if [ ! -x "./run_webserv_ubsan.sh" ]; then
            echo "❌ Error: ./run_webserv_ubsan.sh not found or not executable."
            exit 1
        fi
        shift
        exec ./run_webserv_ubsan.sh "$@"
        ;;
    *)
        exec ./bin/webserv "$@"
        ;;
esac
