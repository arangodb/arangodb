#!/bin/bash

INPUT="$1"
OUTPUT="Doxygen/wiki/`basename $INPUT`"

cp $INPUT $OUTPUT
