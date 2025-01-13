#!/bin/sh
# Finder app
# Author: Jack Center

filesdir=$1     # The directory to search
searchstr=$2    # The string to search for

# return 1 if either argument is missing 
if ! [ $# -eq 2 ]
then
    echo "Error: Expected 2 arguments, got ${#}."
    exit 1
fi

# return 1 "filesdir does not represent a diretory on the filesystem"
if ! [ -d ${filesdir} ]
then
    echo "Error: The directory '${filesdir}' does not exist."
    exit 1
fi

files_with_string=$(grep -rl "$searchstr" "$filesdir" | wc -l)
lines_with_string=$(grep -r "$searchstr" "$filesdir" | wc -l)

echo "The number of files are ${files_with_string} and the number of matching lines are ${lines_with_string}"
exit 0