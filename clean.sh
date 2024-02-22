#!/bin/bash

# List shared memory segments with their IDs
segments=$(ipcs -m | awk '/^0x/ {print $2}')

# Remove each segment
for seg in $segments; do
    ipcrm -m "$seg"
    echo "Removed shared memory segment: $seg"
done

echo "All shared memory segments removed."

