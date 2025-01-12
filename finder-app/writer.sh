#!/bin/sh
# Writer
# Author: Jack Center

# arg1 path to a directory: filesdir
# arg2 text to search for: searchstr
writefile=$1
writestr=$2 

# return 1 if either argument is missing 
if ! [ $# -eq 2 ]
then
    exit 1
fi

if ! [ -f ${writefile} ]
then
    mkdir -p "$(dirname "$writefile")"
    touch "$writefile"
fi

echo "$writestr" > "$writefile"
exit 0