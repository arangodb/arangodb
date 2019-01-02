#!/bin/sh

ulimit -c 0
"$@" || true
