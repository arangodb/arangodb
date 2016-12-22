#!/bin/sh

ulimit -c 0
exec $@
