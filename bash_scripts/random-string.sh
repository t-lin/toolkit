#!/bin/bash
if [[ $# -ne 1 ]]; then
    echo "ERROR: Expecting only one argument (string length)"
    exit 1
else
    LENGTH=$1
fi

if [[ ${LENGTH} -gt 0 ]]; then
    # tr's dc option := Delete the opposite of the set (i.e. leave only the set)
    head /dev/urandom | tr -dc A-Za-z | head -c${LENGTH}
    echo
else
    echo "ERROR: Length must be a positive integer"
fi
