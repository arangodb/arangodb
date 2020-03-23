#!/usr/bin/env bash

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

export VERBOSE=0

main(){
  if [ "$1" == "-v" ] ; then
    export VERBOSE=1
    shift
  fi
  local ARANGOD_EXECUTABLE=${1-build/bin/arangod}

  #delete all existing events
  perf probe -x $ARANGOD_EXECUTABLE -d "probe_arangod:*"

  echo "Adding events, this takes a few seconds..."

  echo "Single document operations..."
  addEvent insertLocal insertLocal@Transaction.cpp
  addEvent removeLocal removeLocal@Transaction.cpp
  addEvent modifyLocal modifyLocal@Transaction.cpp
  addEvent documentLocal documentLocal@Transaction.cpp

  echo "Single document operations on coordinator..."
  addEvent insertCoordinator insertCoordinator@Transaction.cpp
  addEvent removeCoordinator removeCoordinator@Transaction.cpp
  addEvent updateCoordinator updateCoordinator@Transaction.cpp
  addEvent replaceCoordinator replaceCoordinator@Transaction.cpp
  addEvent documentCoordinator documentCoordinator@Transaction.cpp
  # For the Enterprise Edition:
  addEvent insertCoordinator insertCoordinator@TransactionEE.cpp
  addEvent removeCoordinator removeCoordinator@TransactionEE.cpp
  addEvent updateCoordinator updateCoordinator@TransactionEE.cpp
  addEvent replaceCoordinator replaceCoordinator@TransactionEE.cpp
  addEvent documentCoordinator documentCoordinator@TransactionEE.cpp

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

  echo "trace R/W locks"
  #addEvent TRI_ReadLockReadWriteLock TRI_ReadLockReadWriteLock@locks-posix.cpp
  #addEvent TRI_WriteLockReadWriteLock TRI_WriteLockReadWriteLock@locks-posix.cpp
  #addEvent TRI_ReadUnlockReadWriteLock TRI_ReadUnlockReadWriteLock@locks-posix.cpp
  #addEvent TRI_WriteUnlockReadWriteLock TRI_WriteUnlockReadWriteLock@locks-posix.cpp
  #addEvent TRI_TryWriteLockReadWriteLock TRI_TryWriteLockReadWriteLock@locks-posix.cpp
  addEvent beginWriteTimed beginWriteTimed@LogicalCollection.cpp

  echo "Some probes in the storage engine:"
  addEvent insertLocalLine1994 LogicalCollection.cpp:1994 noRet
  addEvent insertLocalLine2013 LogicalCollection.cpp:2013 noRet
  addEvent insertLocalLine2028 LogicalCollection.cpp:2028 noRet
  addEvent insertLocalLine2046 LogicalCollection.cpp:2046 noRet
  addEvent insertLocalLine2050 LogicalCollection.cpp:2050 noRet
  addEvent insertLocalLine2053 LogicalCollection.cpp:2053 noRet
  addEvent insertLocalLine2062 LogicalCollection.cpp:2062 noRet

  echo "Mutexes"
  #addEvent MutexLock lock@Mutex.cpp
  echo Done.
}

addEvent() {
  local name="$1"
  local func="${2-"${name}"}"
  local withRet="${3-1}"

  echo "setting up $name for function: $func"
  if [ "$VERBOSE" == "1" ] ; then
    perf probe -x $ARANGOD_EXECUTABLE -a $name=$func                #enter function
    if [ "$withRet" == 1 ]; then
        perf probe -x $ARANGOD_EXECUTABLE -a ${name}Ret=$func%return    #return form function
    fi
  else
    perf probe -x $ARANGOD_EXECUTABLE -a $name=$func 2> /dev/null             #enter function
    if [ "$withRet" == 1 ]; then
        perf probe -x $ARANGOD_EXECUTABLE -a ${name}Ret=$func%return 2> /dev/null #return form function
    fi
  fi
  if [ "$?" != "0" ] ; then
    echo ERROR!
  fi
}

main "$@"
