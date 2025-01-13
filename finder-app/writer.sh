#!/bin/sh
# Writer
# Author: Jack Center

writefile=$1    # File to write to
writestr=$2     # String to write

# return 1 if either argument is missing 
if ! [ $# -eq 2 ]
then
    echo "Error: The directory '${filesdir}' does not exist."
    exit 1
fi

if ! [ -f ${writefile} ]
then
    mkdir -p "$(dirname "$writefile")"
    touch "$writefile"
fi

echo "$writestr" > "$writefile"
exit 0