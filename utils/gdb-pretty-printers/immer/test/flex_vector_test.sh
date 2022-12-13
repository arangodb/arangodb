#!/bin/sh

flex_vector_test_executable="$1"
flex_vector_test_gdbscript="$2"

gdb "$flex_vector_test_executable" -batch -x "$flex_vector_test_gdbscript"
rv=$?

exit $rv
