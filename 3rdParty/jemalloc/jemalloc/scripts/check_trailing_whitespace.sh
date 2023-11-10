#!/usr/bin/env bash

if git grep -E '\s+$' -- ':!*.md' ':!build-aux/install-sh'
then
	echo 'Error: found trailing whitespace' 1>&2
	exit 1
fi
