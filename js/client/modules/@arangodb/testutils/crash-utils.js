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
const {
  executeExternal,
  executeExternalAndWait,
  statusExternal,
  killExternal,
  statisticsExternal,
  platform,
  sleep
} = internal;
const pu = require('@arangodb/testutils/process-utils');

const abortSignal = 6;
const termSignal = 15;


const CYAN = internal.COLORS.COLOR_CYAN;
const RED = internal.COLORS.COLOR_RED;
const GREEN = internal.COLORS.COLOR_GREEN;
const RESET = internal.COLORS.COLOR_RESET;

let GDB_OUTPUT = '';

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

  let command = 'ulimit -c 0; sleep 10;(';
  // send some line breaks in case of gdb wanting to paginate...
  command += 'printf \'\\n\\n\\n\\n' +
    'set pagination off\\n' +
    'set confirm off\\n' +
    'set logging file ' + gdbOutputFile + '\\n' +
    'set logging on\\n' +
    'bt\\n' +
    'thread apply all bt\\n'+
    'bt full\\n' +
    '\';';

  command += 'sleep 10;';
  command += 'echo quit;';
  command += 'sleep 2';
  command += ') | gdb ' + storeArangodPath + ' ';

  if (options.coreDirectory === '') {
    command += 'core';
  } else {
    command += options.coreDirectory;
  }

  const args = ['-c', command];
  print(JSON.stringify(args));
  
  sleep(5);
  executeExternalAndWait('/bin/bash', args);
  GDB_OUTPUT += `--------------------------------------------------------------------------------
Crash analysis of: ` + JSON.stringify(instanceInfo.getStructure()) + '\n';
  if (!fs.exists(gdbOutputFile)) {
    print("Failed to generate GDB output file?");
    return "";
  }
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


function generateCoreDumpGDB (instanceInfo, options, storeArangodPath, pid, generateCoreDump) {
  let gdbOutputFile = fs.getTempFile();
  let gcore = '';
  if (generateCoreDump) {
    if (options.coreDirectory === '') {
      gcore = `generate-core-file core.${instanceInfo.pid}\\n`;
    } else {
      gcore = `generate-core-file ${options.coreDirectory}\\n`;
    }
  }
  let command = 'ulimit -c 0; sleep 10;(';
  // send some line breaks in case of gdb wanting to paginate...
  command += 'printf \'\\n\\n\\n\\n' +
    'set pagination off\\n' +
    'set confirm off\\n' +
    'set logging file ' + gdbOutputFile + '\\n' +
    'set logging on\\n' +
    'bt\\n' +
    'thread apply all bt\\n'+
    'bt full\\n' +
    gcore +
    'kill \\n' +
    '\';';

  command += 'sleep 10;';
  command += 'echo quit;';
  command += 'sleep 2';
  command += ') | gdb ' + storeArangodPath + ' -p ' + instanceInfo.pid;


  const args = ['-c', command];
  print(JSON.stringify(args));
  command = 'gdb ' + storeArangodPath + ' ';

  if (options.coreDirectory === '') {
    command += 'core';
  } else {
    command += options.coreDirectory;
  }
  return {
    pid: executeExternal('/bin/bash', args),
    file: gdbOutputFile,
    hint: command,
    verbosePrint: true
  };
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
Crash analysis of: ` + JSON.stringify(instanceInfo.getStructure()) + '\n';
  let thisDump = fs.read(lldbOutputFile);
  GDB_OUTPUT += thisDump;
  if (options.extremeVerbosity === true) {
    print(thisDump);
  }
  return 'lldb ' + storeArangodPath + ' -c /cores/core.' + pid;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief analyzes a core dump using lldb (macos)
// /
// / We assume the system has core files in /cores/, and we have a lldb.
// //////////////////////////////////////////////////////////////////////////////

function generateCoreDumpMac (instanceInfo, options, storeArangodPath, pid, generateCoreDump) {
  let lldbOutputFile = fs.getTempFile();
  let gcore = '';
  if (generateCoreDump) {
    if (options.coreDirectory === '') {
      gcore = ` process save-core core.${instanceInfo.pid}\\n`;
    } else {
      gcore = ` process save-core ${options.coreDirectory}/core.${instanceInfo.pid}\\n`;
    }
  }

  let command;
  command = '(';
  command += 'printf \'bt \n\n';
  // LLDB doesn't have an equivilant of `bt full` so we try to show the upper
  // most 5 frames with all variables
  for (var i = 0; i < 5; i++) {
    command += 'frame variable\\n up \\n';
  }
  command += ` thread backtrace all\\n`;
  command += gcore;
  command += ` kill\\n';`;
  command += 'sleep 10;';
  command += 'echo quit;';
  command += 'sleep 2';
  command += ') | lldb ';
  command += ` --attach-pid ${pid} `;
  command += storeArangodPath;
  command += ' > ' + lldbOutputFile + ' 2>&1';
  const args = ['-c', command];
  print(JSON.stringify(args));
  return {
    pid: executeExternal('/bin/bash', args),
    file: lldbOutputFile,
    hint:  'lldb ' + storeArangodPath + ' -c /cores/core.' + pid,
    verbosePrint: true
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief analyzes a core dump using cdb (Windows)
// /  cdb is part of the WinDBG package.
// //////////////////////////////////////////////////////////////////////////////


// //////////////////////////////////////////////////////////////////////////////
// / @brief check whether process does bad on the wintendo
// //////////////////////////////////////////////////////////////////////////////

function runProcdump (options, instanceInfo, rootDir, pid, instantDump = false) {
  let procdumpArgs = [ ];
  let dumpFile = fs.join(rootDir, 'core_' + pid + '.dmp');
  if (options.exceptionFilter != null) {
    procdumpArgs = [
      '-accepteula',
      '-64',
    ];
    if (!instantDump) {
      procdumpArgs.push('-e');
      procdumpArgs.push(options.exceptionCount);
    }
    let filters = options.exceptionFilter.split(',');
    for (let which in filters) {
      procdumpArgs.push('-f');
      procdumpArgs.push(filters[which]);
    }
    procdumpArgs.push('-ma');
    procdumpArgs.push(pid);
    procdumpArgs.push(dumpFile);
  } else {
    procdumpArgs = [
      '-accepteula',
    ];
    if (!instantDump) {
      procdumpArgs.push('-e');
    }
    procdumpArgs.push('-ma');
    procdumpArgs.push(pid);
    procdumpArgs.push(dumpFile);
  }
  try {
    if (options.extremeVerbosity) {
      print(Date() + " Starting procdump: " + JSON.stringify(procdumpArgs));
    }
    instanceInfo.coreFilePattern = dumpFile;
    if (instantDump) {
      // Wait for procdump to have written the dump before we attempt to kill the process:
      instanceInfo.monitor = executeExternalAndWait('procdump', procdumpArgs);
    } else {
      instanceInfo.monitor = executeExternal('procdump', procdumpArgs);
      // try to give procdump a little time to catch up with the process
      sleep(0.25);
      let status = statusExternal(instanceInfo.monitor.pid, false);
      if (status.hasOwnProperty('signal')) {
        print(RED + 'procdump didn\'t come up: ' + JSON.stringify(status));
        instanceInfo.monitor.status = status;
        return false;
      }
    }
  } catch (x) {
    print(Date() + ' failed to start procdump - is it installed?');
    // throw x;
    return false;
  }
  return true;
}

function stopProcdump (options, instanceInfo, force = false) {
  if (instanceInfo.hasOwnProperty('monitor') &&
      instanceInfo.monitor.pid !== null) {
    if (force) {
      print(Date() + " sending TERM to procdump to make it exit");
      instanceInfo.monitor.status = killExternal(instanceInfo.monitor.pid, termSignal);
    } else {
      print(Date() + " waiting for procdump to exit");
      statusExternal(instanceInfo.monitor.pid, true);
    }
    instanceInfo.monitor.pid = null;
  }
}

function calculateMonitorValues(options, instanceInfo, pid, cmd) {
  
  if (platform.substr(0, 3) === 'win') {
    if (process.env.hasOwnProperty('COREDIR')) {
      instanceInfo.coreFilePattern = fs.join(process.env['COREDIR'],
                                             'core_' + pid.toString() + '.dmp');
      instanceInfo.coreDirectory = process.env['COREDIR'];
    } else if (process.env.hasOwnProperty('WORKSPACE') &&
        fs.isDirectory(fs.join(process.env['WORKSPACE'], 'core'))) {
      let spcmd = fs.normalize(cmd).split(fs.pathSeparator);
      let executable = spcmd[spcmd.length - 1];
      instanceInfo.coreFilePattern = fs.join(process.env['WORKSPACE'],
                                             'core',
                                             executable + '.' + pid.toString() + '.dmp');
      instanceInfo.coreDirectory = fs.join(process.env['WORKSPACE'], 'core');
    }
  }
}
function isEnabledWindowsMonitor(options, instanceInfo, pid, cmd) {
  calculateMonitorValues(options, instanceInfo, pid, cmd);
  if (platform.substr(0, 3) === 'win' && !options.disableMonitor) {
    return true;
  }
  return false;
}

const core_rx = new RegExp('core_.*_.*.dmp');
function analyzeCoreDumpWindows (instanceInfo) {
  let cdbOutputFile = fs.getTempFile();

  if (!fs.exists(instanceInfo.coreFilePattern)) {
    const m = 'core file ' + instanceInfo.coreFilePattern + ' not found?';
    instanceInfo.coreFilePattern = null;
    // locate arangods crash handler self generated minidump files:
    fs.list(instanceInfo.coreDirectory).forEach(file => {
      if (file.match(core_rx) !== null) {
        instanceInfo.coreFilePattern = fs.join(instanceInfo.coreDirectory, file);
      }
    });
    if (instanceInfo.coreDirectory === null) {
      print(m);
      return;
    }
  }
  print('found minidump file: ' + instanceInfo.coreFilePattern);

  const dbgCmds = [
    '.logopen ' + cdbOutputFile,
    '!analyze -v', // print verbose analysis
    'dv', // analyze local variables (if)
    '~*kb', // print all threads stack traces
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
Crash analysis of: ` + JSON.stringify(instanceInfo.getStructure()) + '\n';
  // cdb will output to stdout anyways, so we can't turn this off here.
  GDB_OUTPUT += fs.read(cdbOutputFile);
  return 'cdb ' + args.join(' ');
}

function generateCoreDumpWindows (instanceInfo) {
  let cdbOutputFile = fs.getTempFile();
  let dbgCmds = [
    '.logopen ' + cdbOutputFile,
    '!analyze -v', // print verbose analysis
    'dv', // analyze local variables (if)
    '~*kb', // print all threads stack traces
    `.dump /mFhtipcy ${instanceInfo.coreFilePattern}`, // write a compact dump file
    '.kill', // terminate the debugged process
    'q' // quit the debugger
  ];

  const args = [
    '-p',
    instanceInfo.pid,
    '-lines',
    '-logo',
    cdbOutputFile,
    '-c',
    dbgCmds.join('; ')
  ];

  print('running cdb ' + JSON.stringify(args));
  process.env['_NT_DEBUG_LOG_FILE_OPEN'] = cdbOutputFile;
  return {
    pid: executeExternal('cdb', args),
    file: cdbOutputFile,
    hint: 'cdb ' + args.join(' '),
    verbosePrint: false
  };
}

function checkMonitorAlive (binary, instanceInfo, options, res) {
  if (instanceInfo.hasOwnProperty('monitor') ) {
    // Windows: wait for procdump to do its job...
    if (!instanceInfo.monitor.hasOwnProperty('status')) {
      let rc = statusExternal(instanceInfo.monitor.pid, false);
      if (rc.status !== 'RUNNING') {
        print(RED + "Procdump of " + instanceInfo.pid + " is gone: " + rc + RESET);
        instanceInfo.monitor = rc;
        // procdump doesn't set proper exit codes, check for
        // dumps that may exist:
        if (fs.exists(instanceInfo.coreFilePattern)) {
          print("checkMonitorAlive: marking crashy");
          instanceInfo.monitor.monitorExited = true;
          instanceInfo.monitor.pid = null;
          pu.serverCrashed = true;
          options.cleanup = false;
          instanceInfo['exitStatus'] = {};
          analyzeCrash(binary, instanceInfo, options, "the process monitor commanded error");
          Object.assign(instanceInfo.exitStatus,
                        killExternal(instanceInfo.pid, abortSignal));
          return false;
        }
      }
    }
    else return instanceInfo.monitor.exitStatus;
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
      yaml.safeDump(instanceInfo.getStructure()) +
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
      let found = false;
      options.coreDirectory = cp.replace('%e', '.*').replace('%t', '.*').replace('%p', instanceInfo.pid).trim();
      if (options.coreDirectory.search('/') < 0) {
        let rx = new RegExp(options.coreDirectory);
        fs.list('.').forEach((file) => {
          if (file.match(rx) != null) {
            options.coreDirectory = file;
            print(GREEN + `Found ${file} - starting analysis` + RESET);
            found = true;
          }
        });
      }
      if (!found) {
        print(RED + 'Don\'t know howto locate corefiles in your system. "' + cpf + '" contains: "' + cp + '" was looking in: "' + options.coreDirectory + RESET);
        print(RED + 'Directory: ' + JSON.stringify(fs.list('.')));
        return;
      }
    }
  }

  print(RED + message + RESET);

  sleep(5);

  let hint = '';
  if (platform.substr(0, 3) === 'win') {
    if (instanceInfo.hasOwnProperty('monitor')) {
      stopProcdump(options, instanceInfo);
    }
    if (!instanceInfo.hasOwnProperty('coreFilePattern') ) {
      print("your process wasn't monitored by procdump, won't have a coredump!");
      instanceInfo.exitStatus['gdbHint'] = "coredump unavailable";
      return;
    } else if (!fs.exists(instanceInfo.coreFilePattern)) {
      print("No coredump exists at " + instanceInfo.coreFilePattern);
      instanceInfo.exitStatus['gdbHint'] = "coredump unavailable";
      return;
    }
    hint = analyzeCoreDumpWindows(instanceInfo);
  } else if (platform === 'darwin') {
    hint = analyzeCoreDumpMac(instanceInfo, options, binary, instanceInfo.pid);
  } else {
    hint = analyzeCoreDump(instanceInfo, options, binary, instanceInfo.pid);
  }
  instanceInfo.exitStatus.gdbHint = 'Run debugger with "' + hint + '"';

}

function generateCrashDump (binary, instanceInfo, options, checkStr) {
  if (instanceInfo.hasOwnProperty('debuggerInfo')) {
    throw new Error("this process is already debugged: " + JSON.stringify(instanceInfo.getStructure()));
  }
  const stats = statisticsExternal(instanceInfo.pid);
  // picking some arbitrary number of a running arangod doubling it
  const generateCoreDump = (
    stats.virtualSize  < 310000000 &&
    stats.residentSize < 140000000
  ) || stats.virtualSize === 0;
  if (options.test !== undefined) {
    print(CYAN + instanceInfo.name + " - in single test mode, hard killing." + RESET);
    instanceInfo.exitStatus = killExternal(instanceInfo.pid, termSignal);
  } else if (platform.substr(0, 3) === 'win') {
    if (!options.disableMonitor) {
      stopProcdump(options, instanceInfo, true);
    }
    instanceInfo.debuggerInfo = generateCoreDumpWindows(instanceInfo);
    instanceInfo.exitStatus = { status: 'TERMINATED'};
  } else if (platform === 'darwin') {
    instanceInfo.debuggerInfo = generateCoreDumpMac(instanceInfo, options, binary, instanceInfo.pid, generateCoreDump);
    instanceInfo.exitStatus = { status: 'TERMINATED'};
  } else {
    instanceInfo.debuggerInfo = generateCoreDumpGDB(instanceInfo, options, binary, instanceInfo.pid, generateCoreDump);
    instanceInfo.exitStatus = { status: 'TERMINATED'};
  }
}

function aggregateDebugger(instanceInfo, options) {
  print("collecting debugger info for: " + JSON.stringify(instanceInfo.getStructure()));
  if (!instanceInfo.hasOwnProperty('debuggerInfo')) {
    print("No debugger info persisted to " + instanceInfo.debuggerInfo.getStructure());
    return false;
  }
  print("waiting for debugger to terminate: " + JSON.stringify(instanceInfo.debuggerInfo));
  let tearDownTimeout = 180; // s
  while (tearDownTimeout > 0) {
    let ret = statusExternal(instanceInfo.debuggerInfo.pid.pid, false);
    print(ret);
    if (ret.status === "RUNNING") {
      sleep(1);
      tearDownTimeout -= 1;
    } else {
      break;
    }
  }
  if (tearDownTimeout <= 0) {
    print(RED+"killing debugger since it did not finish its busines in 180s"+RESET);
    killExternal(instanceInfo.debuggerInfo.pid.pid, termSignal);
    print(statusExternal(instanceInfo.debuggerInfo.pid.pid, false));
  }
  if (!fs.exists(instanceInfo.debuggerInfo.file)) {
    print("Failed to generate the debbugers output file for " +
          JSON.stringify(instanceInfo.getStructure()) + '\n');
    return "";
  }

  GDB_OUTPUT += `
--------------------------------------------------------------------------------
Crash analysis of: ` + JSON.stringify(instanceInfo.getStructure()) + '\n';
  const buf = fs.readBuffer(instanceInfo.debuggerInfo.file);
  let lineStart = 0;
  let maxBuffer = buf.length;

  for (let j = 0; j < maxBuffer; j++) {
    if (buf[j] === 10) { // \n
      const line = buf.asciiSlice(lineStart, j);
      lineStart = j + 1;
      if (line.search('bytes of data for memory region at') !== -1) {
        continue;
      }
      GDB_OUTPUT += line + '\n';
      if (options.extremeVerbosity === true && instanceInfo.debuggerInfo.verbosePrint) {
        print(line);
      }
    }
  }
  return instanceInfo.debuggerInfo.hint;
}

exports.aggregateDebugger = aggregateDebugger;
exports.generateCrashDump = generateCrashDump;
exports.checkMonitorAlive = checkMonitorAlive;
exports.analyzeCrash = analyzeCrash;
exports.runProcdump = runProcdump;
exports.stopProcdump = stopProcdump;
exports.isEnabledWindowsMonitor = isEnabledWindowsMonitor;
exports.calculateMonitorValues = calculateMonitorValues;
Object.defineProperty(exports, 'GDB_OUTPUT', {get: () => GDB_OUTPUT});
