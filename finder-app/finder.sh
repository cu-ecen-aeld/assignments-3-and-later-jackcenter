#!/bin/sh
# Finder app
# Author: Jack Center

# arg1 path to a directory: filesdir
# arg2 text to search for: searchstr
filesdir=$1
searchstr=$2 

# return 1 if either argument is missing 
if ! [ $# -eq 2 ]
then
    exit 1
fi

# return 1 "filesdir does not represent a diretory on the filesystem"
if ! [ -d ${filesdir} ]
then
    exit 1
fi

# prints The number of files are X and the number of matching lines are Y"
files_with_string=$(grep -rl "$searchstr" "$filesdir" | wc -l)
lines_with_string=$(grep -r "$searchstr" "$filesdir" | wc -l)

echo "The number of files are ${files_with_string} and the number of matching lines are ${lines_with_string}"
exit 0