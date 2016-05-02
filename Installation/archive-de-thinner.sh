#!/bin/bash
# this shell script takes all current .ar - archives it finds.
# if they're thin (only contain references to the object files)
# it creates a new .ar - archive with the referenced objects
# and overwrites the original .a 
# takes one argument: the directory to spider the .a files.

for lib in `find $1 -name '*.a'`; do
    (for file in `ar -t ${lib}`; do 
        find -name ${file}
    done) |ar rvs ${lib}.new ${FN} ||exit 1
done
