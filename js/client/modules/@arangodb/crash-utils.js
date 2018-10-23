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
const internal = require('internal');
const executeExternalAndWait = internal.executeExternalAndWait;
const statusExternal = internal.statusExternal;
const killExternal = internal.killExternal;
const sleep = internal.sleep;
const pu = require('@arangodb/process-utils');

let GDB_OUTPUT = '';
const abortSignal = 6;

const platform = internal.platform;

const RED = internal.COLORS.COLOR_RED;
const RESET = internal.COLORS.COLOR_RESET;

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
  GDB_OUTPUT += `--------------------------------------------------------------------------------
Crash analysis of: ` + JSON.stringify(instanceInfo) + '\n';
  let thisDump = fs.read(gdbOutputFile);
  GDB_OUTPUT += thisDump;
  if (options.extremeVerbosity === true) {
    print(thisDump);
  }

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
  GDB_OUTPUT += `--------------------------------------------------------------------------------
Crash analysis of: ` + JSON.stringify(instanceInfo) + '\n';
  let thisDump = fs.read(lldbOutputFile);
  GDB_OUTPUT += thisDump;
  if (options.extremeVerbosity === true) {
    print(thisDump);
  }
  return 'lldb ' + storeArangodPath + ' -c /cores/core.' + pid;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief analyzes a core dump using cdb (Windows)
// /  cdb is part of the WinDBG package.
// //////////////////////////////////////////////////////////////////////////////

function analyzeCoreDumpWindows (instanceInfo) {
  let cdbOutputFile = fs.getTempFile();

  if (!fs.exists(instanceInfo.coreFilePattern)) {
    print('core file ' + instanceInfo.coreFilePattern + ' not found?');
    return;
  }


  const dbgCmds = [
    '.logopen ' + cdbOutputFile,
    'kp', // print curren threads backtrace with arguments
    '~*kb', // print all threads stack traces
    'dv', // analyze local variables (if)
    '!analyze -v', // print verbose analysis
    'q' // quit the debugger
  ];

  const args = [
    '-z',
    instanceInfo.coreFilePattern,
    '-lines',
    '-logo',
    cdbOutputFile,
    '-c',
    dbgCmds.join('; ')
  ];

  sleep(5);
  print('running cdb ' + JSON.stringify(args));
  process.env['_NT_DEBUG_LOG_FILE_OPEN'] = cdbOutputFile;
  executeExternalAndWait('cdb', args);
  GDB_OUTPUT += `--------------------------------------------------------------------------------
Crash analysis of: ` + JSON.stringify(instanceInfo) + '\n';
  // cdb will output to stdout anyways, so we can't turn this off here.
  GDB_OUTPUT += fs.read(cdbOutputFile);
  return 'cdb ' + args.join(' ');
}

function checkMonitorAlive (binary, arangod, options, res) {
  if (arangod.hasOwnProperty('monitor') ) {
    // Windows: wait for procdump to do its job...
    if (!arangod.monitor.hasOwnProperty('status')) {
      let rc = statusExternal(arangod.monitor.pid, false);
      if (rc.status !== 'RUNNING') {
        arangod.monitor = rc;
        // procdump doesn't set propper exit codes, check for
        // dumps that may exist:
        if (fs.exists(arangod.coreFilePattern)) {
          print("checkMonitorAlive: marking crashy");
          arangod.monitor.monitorExited = true;
          arangod.monitor.pid = null;
          pu.serverCrashed = true;
          arangod['exitStatus'] = {};
          analyzeCrash(binary, arangod, options, "the process monitor commanded error");
          Object.assign(arangod.exitStatus,
                        killExternal(arangod.pid, abortSignal));
          return false;
        }
      }
    }
    else return arangod.monitor.exitStatus;
  }
  return true;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief the bad has happened, tell it the user and try to gather more
// /        information about the incident.
// //////////////////////////////////////////////////////////////////////////////
function analyzeCrash (binary, instanceInfo, options, checkStr) {
  if (instanceInfo.exitStatus.hasOwnProperty('gdbHint')) {
    print(RESET);
    return;
  }
  let message = 'during: ' + checkStr + ': Core dump written; ' +
        /*
        'copying ' + binary + ' to ' +
        storeArangodPath + ' for later analysis.\n' +
        */
        'Process facts :\n' +
        yaml.safeDump(instanceInfo) +
      'marking build as crashy.';
  pu.serverFailMessages = pu.serverFailMessages + '\n' + message;

  if (!options.coreCheck) {
    instanceInfo.exitStatus['gdbHint'] = message;
    print(RESET);
    return;
  }
  var cpf = '/proc/sys/kernel/core_pattern';

  if (fs.isFile(cpf)) {
    var matchApport = /.*apport.*/;
    var matchVarTmp = /\/var\/tmp/;
    var matchSystemdCoredump = /.*systemd-coredump*/;
    var corePattern = fs.readBuffer(cpf);
    var cp = corePattern.asciiSlice(0, corePattern.length);

    if (matchApport.exec(cp) !== null) {
      print(RED + 'apport handles corefiles on your system. Uninstall it if you want us to get corefiles for analysis.' + RESET);
      return;
    }

    if (matchSystemdCoredump.exec(cp) !== null) {
      options.coreDirectory = '/var/lib/systemd/coredump/*core*' + instanceInfo.pid + '*';
    } else if (matchVarTmp.exec(cp) !== null) {
      options.coreDirectory = cp.replace('%e', '*').replace('%t', '*').replace('%p', instanceInfo.pid);
    } else {
      print(RED + 'Don\'t know howto locate corefiles in your system. "' + cpf + '" contains: "' + cp + '"' + RESET);
      return;
    }
  }

  let pathParts = binary.split(fs.pathSeparator);
  let bareBinary = binary;
  if (pathParts.length > 0) {
    bareBinary = pathParts[pathParts.length - 1];
  }
  const storeArangodPath = instanceInfo.rootDir + '/' + bareBinary + '_' + instanceInfo.pid;

  print(RED + message + RESET);

  sleep(5);

  let hint = '';
  if (platform.substr(0, 3) === 'win') {
    if (!instanceInfo.hasOwnProperty('monitor')) {
      print("your process wasn't monitored by procdump, won't have a coredump!");
      instanceInfo.exitStatus['gdbHint'] = "coredump unavailable";
      return;
    }
    if (instanceInfo.monitor.pid !== null) {
      instanceInfo.monitor = statusExternal(instanceInfo.monitor.pid, true);
    }
    hint = analyzeCoreDumpWindows(instanceInfo);
  } else if (platform === 'darwin') {
    // fs.copyFile(binary, storeArangodPath);
    hint = analyzeCoreDumpMac(instanceInfo, options, binary, instanceInfo.pid);
  } else {
    // fs.copyFile(binary, storeArangodPath);
    hint = analyzeCoreDump(instanceInfo, options, binary, instanceInfo.pid);
  }
  instanceInfo.exitStatus.gdbHint = 'Run debugger with "' + hint + '"';

}

exports.checkMonitorAlive = checkMonitorAlive;
exports.analyzeCrash = analyzeCrash;
Object.defineProperty(exports, 'GDB_OUTPUT', {get: () => GDB_OUTPUT});
