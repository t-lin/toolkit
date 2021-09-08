#!/bin/bash
if [[ $# -ne 1 ]]; then
    echo "ERROR: Expecting an interval in seconds (e.g. 3)"

else
    # Strip any letters, in case user provided something like "5s"
    # Keep only the first part (e.g. 2.3 => 2)
    INTV=$(echo $1 | grep -E -o "[0-9]+" | head -n1)
fi

echo "INTERVAL IS: ${INTV} SECONDS"

while [[ 1 ]]; do
    date +%s.%N

    # Use nesting to minimize overhead of repeated calculations & variable storing/access
    sleep `echo "scale=9; $(date +%s.0) + ${INTV} - $(date +%s.%N)" | bc`
done
