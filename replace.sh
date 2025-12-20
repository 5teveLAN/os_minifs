#!/bin/bash

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 [old_string] [new_string]"
    exit 1
fi

OLD_NAME="$1"
NEW_NAME="$2"

sed -i "s/\b$OLD_NAME\b/$NEW_NAME/g" *.c *.h

echo "Replaced '$OLD_NAME' with '$NEW_NAME' in all .c and .h files."

