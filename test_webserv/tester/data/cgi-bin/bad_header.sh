#!/usr/bin/env bash
# bad_header.sh — intentionally emits no HTTP headers

# omit any Content-Type or blank line
echo "This CGI is broken: no headers here!"