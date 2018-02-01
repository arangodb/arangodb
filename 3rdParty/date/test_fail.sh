#!/bin/bash

echo $1
eval $1

if [ $? -eq 0 ]; then
	exit 0;
fi
exit 1;

