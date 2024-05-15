#!/bin/sh

echo "Running tests..."
for test in "$@"; do
    echo "Running $test..."
    ./$test
    if [ $? -ne 0 ]; then
        echo "$test failed"
        exit 1
    fi
done
