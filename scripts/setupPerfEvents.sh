#!/bin/bash

# This requires the perf utility to be installed and the OS must be linux.
# Compile with CMAKE_BUILD_TYPE=RelWithDebInfo in the subdirectory "build".
# Run this script in the main source directory.
#
# This script sets up performance monitoring events to measure single
# document operations. Run this script with sudo when the ArangoDB
# process is already running. Then do
#   sudo perf record -e "probe_arangod:*" -aR sleep 60
# (to sample for 60 seconds). A file "perf.data" is written to the 
# current directory.
# Dump the events in this file with
#   sudo perf script
# This logs the times when individual threads hit the events.
# Remove all events with
#   sudo perf probe -d "probe_arangod:*"
# List events with
#   sudo perf probe -l

ARANGOD_EXECUTABLE=build/bin/arangod
perf probe -x $ARANGOD_EXECUTABLE -d "probe_arangod:*"
echo Adding events, this takes a few seconds...
perf probe -x $ARANGOD_EXECUTABLE -a insertLocal 2> /dev/null
perf probe -x $ARANGOD_EXECUTABLE -a insertLocalRet=insertLocal%return 2> /dev/null
perf probe -x $ARANGOD_EXECUTABLE -a removeLocal 2> /dev/null
perf probe -x $ARANGOD_EXECUTABLE -a removeLocalRet=removeLocal%return 2> /dev/null
perf probe -x $ARANGOD_EXECUTABLE -a modifyLocal 2> /dev/null
perf probe -x $ARANGOD_EXECUTABLE -a modifyLocalRet=modifyLocal%return 2> /dev/null
perf probe -x $ARANGOD_EXECUTABLE -a documentLocal 2> /dev/null
perf probe -x $ARANGOD_EXECUTABLE -a documentLocalRet=documentLocal%return 2> /dev/null
perf probe -x $ARANGOD_EXECUTABLE -a insertCoordinator 2> /dev/null
perf probe -x $ARANGOD_EXECUTABLE -a insertCoordinatorRet=insertCoordinator%return 2> /dev/null
perf probe -x $ARANGOD_EXECUTABLE -a removeCoordinator 2> /dev/null
perf probe -x $ARANGOD_EXECUTABLE -a removeCoordinatorRet=removeCoordinator%return 2> /dev/null
perf probe -x $ARANGOD_EXECUTABLE -a updateCoordinator 2> /dev/null
perf probe -x $ARANGOD_EXECUTABLE -a updateCoordinatorRet=updateCoordinator%return 2> /dev/null
perf probe -x $ARANGOD_EXECUTABLE -a replaceCoordinator 2> /dev/null
perf probe -x $ARANGOD_EXECUTABLE -a replaceCoordinatorRet=replaceCoordinator%return 2> /dev/null
perf probe -x $ARANGOD_EXECUTABLE -a documentCoordinator 2> /dev/null
perf probe -x $ARANGOD_EXECUTABLE -a documentCoordinatorRet=documentCoordinator%return 2> /dev/null
echo Done.
