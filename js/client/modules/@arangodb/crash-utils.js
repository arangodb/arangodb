/* jshint strict: false, sub: true */
/* global print */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
// / Copyright 2014 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const fs = require('fs');
const yaml = require('js-yaml');
const executeExternalAndWait = require('internal').executeExternalAndWait;
const statusExternal = require('internal').statusExternal;
const sleep = require('internal').sleep;

let GDB_OUTPUT = '';

const platform = require('internal').platform;

const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;

// //////////////////////////////////////////////////////////////////////////////
// / @brief analyzes a core dump using gdb (Unix)
// /
// / We assume the system has core files in /var/tmp/, and we have a gdb.
// / you can do this at runtime doing:
// /
// / echo 1 > /proc/sys/kernel/core_uses_pid
// / echo /var/tmp/core-%e-%p-%t > /proc/sys/kernel/core_pattern
// /
// / or at system startup by altering /etc/sysctl.d/corepattern.conf :
// / # We want core files to be located in a central location
// / # and know the PID plus the process name for later use.
// / kernel.core_uses_pid = 1
// / kernel.core_pattern =  /var/tmp/core-%e-%p-%t
// /
// / If you set coreDirectory to empty, this behavior is changed: The core file
// / expected to be named simply "core" and should exist in the current
// / directory.
// //////////////////////////////////////////////////////////////////////////////

function analyzeCoreDump (instanceInfo, options, storeArangodPath, pid) {
  let gdbOutputFile = fs.getTempFile();

  let command;
  command = '(';
  command += 'printf \'bt full\\n thread apply all bt\\n\';';
  command += 'sleep 10;';
  command += 'echo quit;';
  command += 'sleep 2';
  command += ') | gdb ' + storeArangodPath + ' ';

  if (options.coreDirectory === '') {
    command += 'core';
  } else {
    command += options.coreDirectory;
  }
  command += ' > ' + gdbOutputFile + ' 2>&1';
  const args = ['-c', command];
  print(JSON.stringify(args));

  sleep(5);
  executeExternalAndWait('/bin/bash', args);
  GDB_OUTPUT = fs.read(gdbOutputFile);
  print(GDB_OUTPUT);

  command = 'gdb ' + storeArangodPath + ' ';

  if (options.coreDirectory === '') {
    command += 'core';
  } else {
    command += options.coreDirectory;
  }
  return command;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief analyzes a core dump using lldb (macos)
// /
// / We assume the system has core files in /cores/, and we have a lldb.
// //////////////////////////////////////////////////////////////////////////////

function analyzeCoreDumpMac (instanceInfo, options, storeArangodPath, pid) {
  let lldbOutputFile = fs.getTempFile();

  let command;
  command = '(';
  command += 'printf \'bt \n\n';
  // LLDB doesn't have an equivilant of `bt full` so we try to show the upper
  // most 5 frames with all variables
  for (var i = 0; i < 5; i++) {
    command += 'frame variable\\n up \\n';
  }
  command += ' thread backtrace all\\n\';';
  command += 'sleep 10;';
  command += 'echo quit;';
  command += 'sleep 2';
  command += ') | lldb ' + storeArangodPath;
  command += ' -c /cores/core.' + pid;
  command += ' > ' + lldbOutputFile + ' 2>&1';
  const args = ['-c', command];
  print(JSON.stringify(args));

  sleep(5);
  executeExternalAndWait('/bin/bash', args);
  GDB_OUTPUT = fs.read(lldbOutputFile);
  print(GDB_OUTPUT);
  return 'lldb ' + storeArangodPath + ' -c /cores/core.' + pid;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief analyzes a core dump using cdb (Windows)
// /  cdb is part of the WinDBG package.
// //////////////////////////////////////////////////////////////////////////////

function analyzeCoreDumpWindows (instanceInfo) {
  const coreFN = instanceInfo.rootDir + '\\' + 'core.dmp';

  if (!fs.exists(coreFN)) {
    print('core file ' + coreFN + ' not found?');
    return;
  }

  const dbgCmds = [
    'kp', // print curren threads backtrace with arguments
    '~*kb', // print all threads stack traces
    'dv', // analyze local variables (if)
    '!analyze -v', // print verbose analysis
    'q' // quit the debugger
  ];

  const args = [
    '-z',
    coreFN,
    '-c',
    dbgCmds.join('; ')
  ];

  sleep(5);
  print('running cdb ' + JSON.stringify(args));
  executeExternalAndWait('cdb', args);

  return 'cdb ' + args.join(' ');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief the bad has happened, tell it the user and try to gather more
// /        information about the incident.
// //////////////////////////////////////////////////////////////////////////////
function analyzeCrash (binary, arangod, options, checkStr) {
  print(RESET);
}

exports.analyzeCrash = analyzeCrash;
Object.defineProperty(exports, 'GDB_OUTPUT', {get: () => GDB_OUTPUT});
