#!/bin/bash

TESTS="/gtest/bin/Debug/model_tests"
OUTPUT_DIR="/gtest/logs"
TIMESTAMP=$(TZ="US/Pacific" date +'%F_%H_%M_%S')
LOG_FILE="$OUTPUT_DIR/valgrind_$TIMESTAMP.log"

if [ ! -d "$OUTPUT_DIR" ]; then
    mkdir -p "$OUTPUT_DIR"
fi

if [ -f "$TESTS" ]; then
    # run valgrind
    valgrind --leak-check=full --log-file="$LOG_FILE" "$TESTS"
else
    echo "$TESTS does not exist. Please build with CMake."
fi
