#!/bin/bash

INCLUDE_FLAGS="-Iinclude -I. -std=c++20"
SRC_DIR="src"
LOG_DIR="iwyu_logs"

mkdir -p "$LOG_DIR"
CPP_FILES=$(find "$SRC_DIR" -name "*.cpp")

for file in $CPP_FILES; do
  echo "🔍 Running iwyu on $file"
  iwyu $INCLUDE_FLAGS "$file" 2>&1 | tee "$LOG_DIR/iwyu_$(basename "$file" .cpp).log"
done
