/* jshint strict: false, sub: true */
/* global print */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
const tu = require('@arangodb/testutils/test-utils');
const versionHas = require("@arangodb/test-helper").versionHas;
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

function filterStack(stack, filters) {
  if (stack.length === 0) {
    return;
  }
  let normalize = (s) => {
    s = s.trim();
    if (s.substr(-3) === "...") {
      s = s.substr(0, s.length - 3);
    }
    return s;
  };
  let lineEqual = (a, b) => {
    a = normalize(a);
    b = normalize(b);
    if (a.length > b.length) {
      return a.substr(0, b.length) === b;
    }
    return b.substr(0, a.length) === a;
  };

  let filtered = false;
  filters.forEach(filter => {
    if (stack.length === filter.length) {
      filtered = filtered || filter.every((filterLine, i) => {
        return lineEqual(filterLine, stack[i]);
      });
    }
  });
  return filtered;
}

function readGdbFileFiltered(gdbOutputFile, options) {
  try {
    const filters = JSON.parse(fs.read(
      fs.join(pu.JS_DIR,
              'client/modules/@arangodb/testutils',
              'filter_gdb_stacks.json')));
    const buf = fs.readBuffer(gdbOutputFile);
    let lineStart = 0;
    let maxBuffer = buf.length;
    let inStack = false;
    let stack = [];
    let longStack = [];
    let moreMessages = [];
    for (let j = 0; j < maxBuffer; j++) {
      if (buf[j] === 10) { // \n
        var line = buf.utf8Slice(lineStart, j);
        lineStart = j + 1;
        if (line.search('Thread ') === 0 || (!inStack && line[0] === '#')) {
          inStack = true;
        }

        if (inStack) {
          if (line[0] === '#') {
            if (line[3] === ' ') {
              line = line.substring(4);
            }
            if (line[0] === '0' && line[1] === 'x') {
              line = line.substring(22);
            }
            longStack.push(line);
            let paramPos = line.search(' \\(');
            if (paramPos !== 0) {
              line = line.substring(0, paramPos);
            }
            if (line.length > 1) {
              stack.push(line);
            }
          } else {
            moreMessages.push(line);
          }
          if (line.length === 0) {
            if (!filterStack(stack, filters)) {
              if (options.extremeVerbosity === true) {
                print("did not filter this stack: ");
                print(stack);
                print(moreMessages);
              }
              moreMessages.forEach(line => {
                GDB_OUTPUT += line.trim() + '\n';
              });
              longStack.forEach(line => {
                GDB_OUTPUT += line.trim() + '\n';
              });
              GDB_OUTPUT += '\n';
            }

            stack = [];
            longStack = [];
            moreMessages = [];
            inStack = false;
          }
        } else {
            GDB_OUTPUT += line.trim() + '\n';
        }
      }
    }
  } catch (ex) {
    let err="failed to read " + gdbOutputFile + " -> " + ex + '\n' + ex.stack;
    print(err);
  }
}

function analyzeCoreDump (instanceInfo, options, storeArangodPath, pid) {
  let gdbOutputFile = fs.getTempFile();

  let command = 'ulimit -c 0; sleep 10;(';
  // send some line breaks in case of gdb wanting to paginate...
  command += 'printf \'\\n\\n\\n\\n' +
    'set pagination off\\n' +
    'set confirm off\\n' +
    'set logging file ' + gdbOutputFile + '\\n' +
    'set logging enabled\\n' +
    'bt\\n' +
    'thread apply all bt\\n'+
    'bt full\\n' +
    '\';';

  command += 'sleep 10;';
  command += 'echo quit;';
  command += 'sleep 2';
  command += ') | nice -17 gdb ' + storeArangodPath + ' ';

  if (options.coreDirectory === '') {
    command += 'core';
  } else {
    command += options.coreDirectory;
  }

  const args = ['-c', command];
  print("launching GDB in foreground: " + JSON.stringify(args));
  sleep(5);
  executeExternalAndWait('/bin/bash', args);
  GDB_OUTPUT += `--------------------------------------------------------------------------------
Crash analysis of: ` + JSON.stringify(instanceInfo.getStructure()) + '\n\n';
  if (!fs.exists(gdbOutputFile)) {
    print("Failed to generate GDB output file?");
    return "";
  }
  readGdbFileFiltered(gdbOutputFile, options);
  if (options.extremeVerbosity === true) {
    let thisDump = fs.read(gdbOutputFile);
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
  let gcore = [];
  if (generateCoreDump) {
    if (options.coreDirectory === '') {
      gcore = ['-ex', `generate-core-file core.${instanceInfo.pid}`];
    } else {
      gcore = ['-ex', `generate-core-file ${options.coreDirectory}`];
    }
  }
  let command = [
    '--batch-silent',
    '-ex', 'set pagination off',
    '-ex', 'set confirm off',
    '-ex', `set logging file ${gdbOutputFile}`,
    '-ex', 'set logging enabled',
    '-ex', 'bt',
    '-ex', 'thread apply all bt',
    '-ex', 'bt full'].concat(gcore).concat([
      '-ex', 'kill',
      storeArangodPath,
      '-p', instanceInfo.pid
    ]);
  print("launching GDB in background: " + JSON.stringify(command));
  return {
    pid: executeExternal('gdb', command),
    file: gdbOutputFile,
    hint: command,
    verbosePrint: true
  };
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
  GDB_OUTPUT += `Analyzing crash of ${instanceInfo.name} PID[${instanceInfo.pid}]: ${checkStr}\n`;
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
    var cp = corePattern.utf8Slice(0, corePattern.length);

    if (matchApport.exec(cp) !== null) {
      print(RED + 'apport handles corefiles on your system. Uninstall it if you want us to get corefiles for analysis.' + RESET);
      return;
    }

    const knownPatterns = ["%s", "%d", "%e", "%E", "%g", "%h", "%i", "%I", "%s", "%t", "%u"];
    const replaceKnownPatterns = (s) => {
      knownPatterns.forEach(p => {
        s = s.replace(p, "*");
      });
      while (true) {
        let oldLength = s.length;
        s = s.replace("**", "*");
        if (s.length === oldLength) { // no more replacements
          break;
        }
      }
      return s;
    };
    if (matchSystemdCoredump.exec(cp) !== null) {
      options.coreDirectory = '/var/lib/systemd/coredump/*core*' + instanceInfo.pid + '*';
    } else if (matchVarTmp.exec(cp) !== null) {
      options.coreDirectory = replaceKnownPatterns(cp).replace('%p', instanceInfo.pid);
    } else {
      let found = false;
      options.coreDirectory = replaceKnownPatterns(cp).replace('%p', instanceInfo.pid).trim();
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
  hint = analyzeCoreDump(instanceInfo, options, binary, instanceInfo.pid);
  instanceInfo.exitStatus.gdbHint = 'Run debugger with "' + hint + '"';

}

function generateCrashDump (binary, instanceInfo, options, checkStr) {
  if (!options.coreCheck && !options.setInterruptable) {
    print(`coreCheck = ${options.coreCheck}; setInterruptable = ${options.setInterruptable}; forcing fatal exit of arangod! Bye!`);
    pu.killRemainingProcesses({status: false});
    process.exit();
  }
  GDB_OUTPUT += `Forced shutdown of ${instanceInfo.name} PID[${instanceInfo.pid}]: ${checkStr}\n`;
  if (instanceInfo.hasOwnProperty('debuggerInfo')) {
    throw new Error("this process is already debugged: " + JSON.stringify(instanceInfo.getStructure()));
  }
  const stats = statisticsExternal(instanceInfo.pid);
  // picking some arbitrary number of a running arangod doubling it
  const generateCoreDump = options.coreGen && ((
    stats.virtualSize  < 310000000 &&
    stats.residentSize < 140000000
  ) || stats.virtualSize === 0);
  if (options.test !== undefined) {
    print(CYAN + instanceInfo.name + " - in single test mode, hard killing." + RESET);
    instanceInfo.exitStatus = killExternal(instanceInfo.pid, termSignal);
  } else {
    instanceInfo.debuggerInfo = generateCoreDumpGDB(instanceInfo, options, binary, instanceInfo.pid, generateCoreDump);
    instanceInfo.exitStatus = { status: 'TERMINATED'};
  }
  // renice debugger to lowest prio so it doesn't steal test resources
  try {
    internal.setPriorityExternal(instanceInfo.debuggerInfo.pid.pid, 20);
  } catch (ex) {
    print(`${RED} renicing of debugger ${instanceInfo.debuggerInfo.pid.pid} failed: ${ex} ${RESET}`);
  }
}

function aggregateDebugger(instanceInfo, options) {
  if (options.extremeVerbosity === true) {
    print("collecting debugger info for: " + JSON.stringify(instanceInfo.getStructure()));
  }
  if (!instanceInfo.hasOwnProperty('debuggerInfo')) {
    print("No debugger info persisted to " + JSON.stringify(instanceInfo.getStructure()));
    return false;
  }
  print(`waiting for debugger of ${instanceInfo.pid} to terminate: ${JSON.stringify(instanceInfo.debuggerInfo)}`);
  let tearDownTimeout = 180; // s
  while (tearDownTimeout > 0) {
    let ret = statusExternal(instanceInfo.debuggerInfo.pid.pid, false);
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
Crash analysis of: ` + JSON.stringify(instanceInfo.getStructure()) + '\n\n';

  readGdbFileFiltered(instanceInfo.debuggerInfo.file, options);
  return instanceInfo.debuggerInfo.hint;
}

exports.aggregateDebugger = aggregateDebugger;
exports.generateCrashDump = generateCrashDump;
exports.analyzeCrash = analyzeCrash;
Object.defineProperty(exports, 'GDB_OUTPUT', { get: () => GDB_OUTPUT, set: (value) => { GDB_OUTPUT = value; }});
exports.registerOptions = function(optionsDefaults, optionsDocumentation) {
  const isSan = versionHas('asan') || versionHas('tsan');
  tu.CopyIntoObject(optionsDefaults, {
    'coreCheck': false,
    'coreAbort': false,
    'coreDirectory': '/var/tmp',
    'coreGen': !isSan,
  });

  tu.CopyIntoList(optionsDocumentation, [
    ' Crash analysis related:',
    '   - `coreGen`: whether debuggers should generate a coredump after getting stacktraces',
    '   - `coreAbort`: if we should use sigAbrt in order to terminate process instead of GDB attaching',
    '   - `coreCheck`: if set to true, we will attempt to locate a coredump to ',
    '                  produce a backtrace in the event of a crash',
    '',
  ]);
};
