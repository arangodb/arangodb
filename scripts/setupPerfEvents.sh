#!/bin/bash

# This requires the perf utility to be installed and the OS must be linux.
# Compile with CMAKE_BUILD_TYPE=RelWithDebInfo in the subdirectory "build".
# Run this script in the main source directory.
#
# This script sets up performance monitoring events to measure single
# document operations. Run this script with sudo when the ArangoDB
# process is already running:
#
#   sudo ./setupPerfEvents.sh
#
# Now you are able to recrod the event with:
#
#   sudo perf record -e "probe_arangod:*" -aR
#
# The above command will get sample data indefinitely, hit Ctrl-C when
# the measurement is finished. A file "perf.data" is written to the
# current directory. Dump the events in this file with:
#
#   sudo perf script > perf.history
#
# This logs the times when individual threads hit the events.
# Use the program perfanalyis.cpp in this directory in the following way:
# (for compilation instructions see at the top of perfanalysis.cpp)
#
#   ./scripts/perfanalyis < perf.history > perf.statistics
#
# This will group enter and exit events of functions together, compute the time
# spent and sort by function. When finished remove all events with:
#
#   sudo perf probe -d "probe_arangod:*"
#
# List events with:
#
#   sudo perf probe -l
#
#

main(){
  local ARANGOD_EXECUTABLE=${1-build/bin/arangod}

  #delete all existing events
  perf probe -x $ARANGOD_EXECUTABLE -d "probe_arangod:*"

  echo "Adding events, this takes a few seconds..."

  echo "Single document operations..."
  addEvent insertLocal
  addEvent removeLocal
  addEvent modifyLocal
  addEvent documentLocal

  echo "Single document operations on coordinator..."
  addEvent insertCoordinator
  addEvent removeCoordinator
  addEvent updateCoordinator
  addEvent replaceCoordinator
  addEvent documentCoordinator

  echo "work method in RestDocumentHandler"
  addEvent executeRestReadDocument readDocument@RestDocumentHandler.cpp
  addEvent executeRestInsertDocument createDocument@RestDocumentHandler.cpp

  echo "work in LogicalCollection"
  addEvent logicalInsert insert@LogicalCollection.cpp

  echo "work in HttpCommTask and GeneralCommTask"
  addEvent processRequest processRequest@HttpCommTask.cpp
  addEvent executeRequest executeRequest@GeneralCommTask.cpp
  addEvent handleRequest handleRequest@GeneralCommTask.cpp
  addEvent handleRequestDirectly handleRequestDirectly@GeneralCommTask.cpp

  echo Done.
}

addEvent() {
  local name="$1"
  local func="${2-"${name}"}"

  echo "setting up $name for function: $func"
  perf probe -x $ARANGOD_EXECUTABLE -a $name=$func 2> /dev/null             #enter function
  perf probe -x $ARANGOD_EXECUTABLE -a ${name}Ret=$func%return 2> /dev/null #return form function
}

main "$@"
