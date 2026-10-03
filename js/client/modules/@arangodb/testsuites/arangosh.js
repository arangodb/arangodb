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
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const time = require('internal').time;
const fs = require('fs');
const yaml = require('js-yaml');

const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const ct = require('@arangodb/testutils/client-tools');
const internal = require('internal');
const toArgv = internal.toArgv;
const statusExternal = internal.statusExternal;
const killExternal = internal.killExternal;
const executeExternal = internal.executeExternal;
const executeExternalAndWait = internal.executeExternalAndWait;
const tmpDirMngr = require('@arangodb/testutils/tmpDirManager').tmpDirManager;
const {sanHandler} = require('@arangodb/testutils/san-file-handler');
const { executeExternalAndWaitWithSanitizer } = require('@arangodb/test-helper');

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const functionsDocumentation = {
  'arangosh': 'arangosh exit codes tests',
};
const testPaths = {
  'arangosh': [],
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief the readPipe scenarios, executed inside a child arangosh
// //////////////////////////////////////////////////////////////////////////////

function readPipeScenarioRunner (scenario, stall) {
  const fs = require('fs');
  const internal = require('internal');

  // sizeof(content) - 1 in JS_ReadPipe(), lib/V8/v8-utils.cpp
  const CHUNK = 1023;
  // SetGlobalExecutionDeadlineTo() takes SECONDS, see
  // client-tools/Shell/v8-deadline.cpp (executionDeadline = TRI_microtime() + n)
  const DEADLINE = 5;
  // deadline aborts raise TRI_ERROR_DISABLED, see the TRI_CreateErrorObject()
  // calls in isExecutionDeadlineReached(), client-tools/Shell/v8-deadline.cpp
  const DEADLINE_ERROR_NUM = internal.errors.ERROR_DISABLED.code;

  // `exec` matters: it makes the pid we track the pid that actually holds the
  // pipe open and stalls, so killing it later does not orphan a sleeper.
  function shell (cmd, usePipes) {
    return internal.executeExternal('/bin/sh', ['-c', cmd], usePipes);
  }

  function reap (res) {
    if (res === undefined || res === null || res.pid === undefined) {
      return;
    }
    try {
      internal.killExternal(res.pid, 9);
    } catch (ignore) {
    }
    try {
      internal.statusExternal(res.pid, true);
    } catch (ignore) {
    }
  }

  // On a build without the interruptible read this never returns for the
  // stalling scenarios.
  function timedRead (pid) {
    const start = internal.time();
    let result = { buf: null, error: null };
    try {
      result.buf = fs.readPipe(pid);
    } catch (ex) {
      result.error = ex;
    }
    result.elapsed = internal.time() - start;
    return result;
  }

  // This has to be strict: getDeadlineReasonString() returns its default
  // "Execution deadline reached!" even when no deadline ever fired, so matching
  // on the reason alone would happily accept a readPipe() that failed for an
  // entirely unrelated cause - such as the process having been deregistered.
  function checkAbort (r, expected) {
    const reason = internal.getDeadlineReasonString();
    const took = r.elapsed.toFixed(1) + 's';

    if (r.error === null) {
      throw new Error('readPipe() returned normally after ' + took + ' with ' +
                      JSON.stringify(r.buf) +
                      ' - expected the deadline to abort the read');
    }

    const detail = 'after ' + took + ', errorNum ' + r.error.errorNum +
        ', reason "' + reason + '" (' + r.error.message + ')';

    if (r.error.errorNum !== DEADLINE_ERROR_NUM) {
      throw new Error('readPipe() failed for an unrelated reason ' + detail +
                      ' - expected errorNum ' + DEADLINE_ERROR_NUM +
                      ' (ERROR_DISABLED)');
    }
    if (reason.indexOf(expected.reason) === -1) {
      throw new Error('aborted ' + detail +
                      ' - expected the reason to contain "' +
                      expected.reason + '"');
    }
    if (r.elapsed < expected.minElapsed || r.elapsed > expected.maxElapsed) {
      throw new Error('aborted ' + detail +
                      ' - expected that to take between ' +
                      expected.minElapsed + 's and ' +
                      expected.maxElapsed + 's');
    }
    return 'aborted ' + detail;
  }

  const scenarios = {
    // A child that emitted a few bytes and then went silent without exiting -
    // the js_driver steady state.
    stalledChild: function () {
      const child = shell('echo hello; exec sleep ' + stall, true);
      print('  pid ' + child.pid + ' wrote 6 of the ' + CHUNK +
            ' bytes readPipe() waits for, and now stays silent');
      internal.SetGlobalExecutionDeadlineTo(DEADLINE);
      const r = timedRead(child.pid);
      internal.SetGlobalExecutionDeadlineTo(0);
      const msg = checkAbort(r, {
        reason: 'Execution deadline reached',
        minElapsed: DEADLINE - 1,
        maxElapsed: DEADLINE + 30
      });
      reap(child);
      return msg;
    },

    // EOF that can never arrive: the direct child exits at once, but a
    // backgrounded grandchild inherited fd 1 and keeps the write end open, so
    // read() never returns 0 even though the process we track is long gone.
    //
    // Do NOT probe with statusExternal() first: TRI_CheckExternalProcess()
    // deletes the ExternalProcess as soon as the status is no longer
    // RUNNING/STOPPED/TIMEOUT, and ~ExternalProcess() closes both pipes. The
    // following readPipe() would then fail with "didn't find the process"
    // instead of blocking, which looks like a pass but proves nothing.
    //
    // This scenario deliberately leaves the grandchild behind - it is not ours
    // to reap, which is exactly what makes the read unblockable.
    noEof: function () {
      const child = shell('(sleep ' + stall + ' &) ; echo bye', true);
      print('  pid ' + child.pid + ' exits immediately, but its grandchild ' +
            'keeps fd 1 open, so read() never sees EOF');
      internal.SetGlobalExecutionDeadlineTo(DEADLINE);
      const r = timedRead(child.pid);
      internal.SetGlobalExecutionDeadlineTo(0);
      const msg = checkAbort(r, {
        reason: 'Execution deadline reached',
        minElapsed: DEADLINE - 1,
        maxElapsed: DEADLINE + 30
      });
      reap(child);
      return msg;
    },

    // Control: a child that writes and exits must still be drained via EOF,
    // with no deadline armed. This is what the pipe test further down relies
    // on, and it must not regress when the read becomes interruptible.
    eofControl: function () {
      const child = shell('echo bye', true);
      const r = timedRead(child.pid);
      reap(child);
      if (r.error !== null) {
        throw new Error('readPipe() threw ' + r.error.message);
      }
      if (r.buf !== 'bye\n') {
        throw new Error('expected "bye\\n", got ' + JSON.stringify(r.buf));
      }
      return 'read ' + JSON.stringify(r.buf) + ' via EOF in ' +
        r.elapsed.toFixed(2) + 's';
    },

    // Control: chunking must stay byte identical. Every caller that loops on
    // `buf.length === 1023` (the js.js / go.js / drivers.js / php.js suites and
    // node-netstat) depends on full buffers being returned as full buffers, and
    // on the final short read being delivered. No deadline is armed here, which
    // is the state those callers are usually in.
    chunkingControl: function () {
      const total = 3000;
      const child = shell('head -c ' + total + ' /dev/zero | tr "\\0" x', true);
      let chunks = [];
      let read = 0;
      let guard = 0;
      while (guard++ < 100) {
        const buf = fs.readPipe(child.pid);
        if (buf.length === 0) {
          break;
        }
        chunks.push(buf.length);
        read += buf.length;
      }
      reap(child);
      const msg = 'chunk sizes: [' + chunks.join(', ') + '], total ' + read;
      if (read !== total) {
        throw new Error(msg + ' - expected ' + total + ' bytes');
      }
      if (chunks.length < 2 || chunks[0] !== CHUNK || chunks[1] !== CHUNK) {
        throw new Error(msg + ' - expected full ' + CHUNK +
                        ' byte chunks until the last one');
      }
      return msg;
    },

    // The CI scenario end to end, without needing a cluster: a monitored
    // process - the stand-in for a dbserver - dies while we drain a silent
    // child. ProcessMonitorThread::run() notices within ~100ms and arms the
    // deadline through triggerV8DeadlineNow().
    monitoredProcessDied: function () {
      const victim = shell('exec sleep ' + stall, false);
      internal.addPidToMonitor(victim.pid);
      const child = shell('echo hello; exec sleep ' + stall, true);
      print('  monitoring pid ' + victim.pid + ' as a stand-in for a ' +
            'dbserver, draining silent pid ' + child.pid);
      internal.killExternal(victim.pid, 9);
      const r = timedRead(child.pid);
      internal.SetGlobalExecutionDeadlineTo(0);
      const msg = checkAbort(r, {
        reason: 'Monitored child process exited unexpectedly',
        minElapsed: 0,
        maxElapsed: 30
      });
      reap(child);
      reap(victim);
      return msg;
    },

    // The same two axes flipped: the monitored process is left to exit
    // *cleanly*, and the child is still actively producing output while we
    // drain it. ProcessMonitorThread::run() arms the deadline for
    // TRI_EXT_TERMINATED just as it does for TRI_EXT_ABORTED, so there is
    // deliberately no kill here.
    monitoredProcessExited: function () {
      const VICTIM_LIFETIME = 3;
      const CADENCE = 0.05;

      const victim = shell('exec sleep ' + VICTIM_LIFETIME, false);
      internal.addPidToMonitor(victim.pid);
      const child = shell('while :; do echo chatter; sleep ' + CADENCE +
                          '; done', true);
      print('  monitoring pid ' + victim.pid + ', which exits cleanly after ' +
            VICTIM_LIFETIME + 's, while draining chatty pid ' + child.pid);
      const r = timedRead(child.pid);
      internal.SetGlobalExecutionDeadlineTo(0);
      const msg = checkAbort(r, {
        reason: 'Monitored child process exited unexpectedly',
        minElapsed: 0.5,
        maxElapsed: 30
      });
      reap(child);
      reap(victim);
      return msg;
    }
  };

  if (!scenarios.hasOwnProperty(scenario)) {
    throw new Error('unknown readPipe scenario: ' + scenario);
  }
  print('  ' + scenario + ': ' + scenarios[scenario]());
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: arangosh
// //////////////////////////////////////////////////////////////////////////////

function arangosh (options) {
  let ret = { failed: 0 };
  [
    'testArangoshExitVersion',
    'testArangoshExitCodeConnectAny',
    'testArangoshExitCodeConnectAnyIp6',
    'testArangoshExitCodeNoConnect',
    'testArangoshExitCodeSyntaxError',
    'testArangoshExitCodeSyntaxErrorInSubScript',
    'testArangoshExitCodeFail',
    'testArangoshExitCodeFailButCaught',
    'testArangoshExitCodeEmpty',
    'testArangoshExitCodeSuccess',
    'testArangoshExitCodeStatements',
    'testArangoshExitCodeStatements2',
    'testArangoshExitCodeNewlines',
    'testArangoshExitCodeEcho',
    'testArangoshShebang'
  ].forEach(function (what) {
    ret[what] = {
      failed: 0,
      status: true,
      total: 0
    };
    ret[what + '_file'] = {
      failed: 0,
      status: true,
      total: 0
    };
  });

  // these have no '_file' variant - the readPipe ones are run by handing a
  // stringified function to a child arangosh, and the pipe-through one drives
  // the child over its stdin/stdout pipes
  [
    'testArangoshPipeThrough',
    'testArangoshReadPipeStalledChild',
    'testArangoshReadPipeNoEof',
    'testArangoshReadPipeEofControl',
    'testArangoshReadPipeChunking',
    'testArangoshReadPipeMonitoredProcessDied',
    'testArangoshReadPipeMonitoredProcessExited'
  ].forEach(function (what) {
    ret[what] = {
      failed: 0,
      status: true,
      total: 0
    };
  });

  // the scenarios arm a 5s deadline; leave generous slack on instrumented
  // builds before we declare the child hung
  const readPipeTimeout = options.isInstrumented ? 180 : 90;
  // the stalling children have to outlive the watchdog, but must not linger for
  // much longer than that when a test fails and they get orphaned
  const readPipeStall = readPipeTimeout * 2;

  function runTest (section, title, command, expectedReturnCode, opts) {
    print('--------------------------------------------------------------------------------');
    print(`testcase ${section} - ${title}`);
    print('--------------------------------------------------------------------------------');

    ////////////////////////////////////////////////////////////////////////////////
    // run command from a .js file
    let args = ct.makeArgs.arangosh(options);
    args['javascript.execute-string'] = command;
    args['log.level'] = 'error';

    for (let op in opts) {
      args[op] = opts[op];
    }

    const startTime = time();
    let rc = executeExternalAndWaitWithSanitizer(pu.ARANGOSH_BIN, toArgv(args), 'arangosh_tests_weird_names', options);
    const deltaTime = time() - startTime;
    const failSuccess = (rc.hasOwnProperty('exit') && rc.exit === expectedReturnCode);

    if (!failSuccess) {
      ret.failed += 1;
      ret[section].failed = 1;
      ret[section]['message'] =
        'didn\'t get expected return code (' + expectedReturnCode + '): \n' +
        yaml.safeDump(rc);
    } else {
      ret[section].failed = 0;
    }

    ++ret[section]['total'];
    ret[section]['status'] = failSuccess;
    ret[section]['duration'] = deltaTime;
    print((failSuccess ? GREEN : RED) + 'Status: ' + (failSuccess ? 'SUCCESS' : 'FAIL') + RESET);
    if (options.extremeVerbosity) {
      print(toArgv(args));
      print(ret[section]);
      print(rc);
      print('expect rc: ' + expectedReturnCode);
      print(`status: ${JSON.stringify(ret[section])}`);
    }

    ////////////////////////////////////////////////////////////////////////////////
    // run command from a .js file
    print('\n--------------------------------------------------------------------------------');
    print(title + ' With js file');
    print('--------------------------------------------------------------------------------');


    var execFile = fs.getTempFile();

    fs.write(execFile, command);
    section += '_file';
    let args2 = ct.makeArgs.arangosh(options);
    args2['javascript.execute'] = execFile;
    args2['log.level'] = 'error';

    for (let op in opts) {
      args2[op] = opts[op];
    }

    const startTime2 = time();

    let rc2 = executeExternalAndWaitWithSanitizer(pu.ARANGOSH_BIN, toArgv(args2), 'arangosh_tests_weird_names_2', options);
    const deltaTime2 = time() - startTime2;
    const failSuccess2 = (rc2.hasOwnProperty('exit') && rc2.exit === expectedReturnCode);

    if (!failSuccess2) {
      ret.failed += 1;
      ret[section].failed = 1;
      ret[section]['message'] =
        'didn\'t get expected return code (' + expectedReturnCode + '): \n' +
        yaml.safeDump(rc2);
    } else {
      ret[section].failed = 0;
    }

    ++ret[section]['total'];
    ret[section]['status'] = failSuccess2;
    ret[section]['duration'] = deltaTime2;
    print((failSuccess2 ? GREEN : RED) + 'Status: ' + (failSuccess2 ? 'SUCCESS' : 'FAIL') + RESET);
    if (options.extremeVerbosity) {
      print(toArgv(args2));
      print(ret[section]);
      print(rc2);
      print('expect rc: ' + expectedReturnCode);
      print(`status: ${JSON.stringify(ret[section])}`);
    }
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief runs one readPipe scenario in its own arangosh, under a timeout.
  // /
  // / We cannot use executeExternalAndWaitWithSanitizer() here: it passes
  // / timeoutms = 0, which takes the blocking waitpid() branch in
  // / TRI_CheckExternalProcess() and would hang this testrun along with the
  // / child. With a non-zero timeout we get the polling branch instead, and a
  // / stuck child surfaces as status 'TIMEOUT' rather than as a dead testrun.
  // //////////////////////////////////////////////////////////////////////////////
  function runReadPipeTest (section, title, scenario) {
    print('--------------------------------------------------------------------------------');
    print(`testcase ${section} - ${title}`);
    print('--------------------------------------------------------------------------------');

    let args = ct.makeArgs.arangosh(options);
    args['server.endpoint'] = 'none';
    args['javascript.allow-external-process-control'] = 'true';
    args['log.level'] = 'error';
    args['javascript.execute-string'] =
      `(${String(readPipeScenarioRunner)})(${JSON.stringify(scenario)}, ${readPipeStall});`;

    let sh = new sanHandler(pu.ARANGOSH_BIN, options);
    let tmpMgr = new tmpDirMngr(`arangosh_tests_readpipe_${scenario}`, options);
    sh.detectLogfiles(tmpMgr.tempDir, tmpMgr.tempDir);

    const startTime = time();
    let rc = executeExternalAndWait(pu.ARANGOSH_BIN, toArgv(args), false,
                                    readPipeTimeout * 1000, sh.getSanOptions());
    const deltaTime = time() - startTime;

    let failSuccess = (rc.hasOwnProperty('exit') && rc.exit === 0);
    let message = '';

    if (rc.status === 'TIMEOUT') {
      // the child is parked in read() and will not come back on its own
      failSuccess = false;
      message =
        'arangosh hung for ' + readPipeTimeout + 's inside fs.readPipe() and ' +
        'had to be killed. It is blocked in read() on the child\'s stdout ' +
        'pipe, so no JavaScript runs and neither the execution deadline nor ' +
        'the process monitoring thread can abort it. See TRI_ReadPointer() in ' +
        'lib/Basics/files.cpp.';
      try {
        killExternal(rc.pid, 9);
      } catch (ignore) {
      }
      try {
        statusExternal(rc.pid, true);
      } catch (ignore) {
      }
    } else if (!failSuccess) {
      message = 'didn\'t get expected return code (0): \n' + yaml.safeDump(rc);
    }

    sh.fetchSanFileAfterExit(rc.pid);

    if (!failSuccess) {
      ret.failed += 1;
      ret[section].failed = 1;
      ret[section]['message'] = message;
    } else {
      ret[section].failed = 0;
    }

    ++ret[section]['total'];
    ret[section]['status'] = failSuccess;
    ret[section]['duration'] = deltaTime;
    print((failSuccess ? GREEN : RED) + 'Status: ' + (failSuccess ? 'SUCCESS' : 'FAIL') + RESET);
    if (!failSuccess) {
      print(RED + message + RESET);
    }
    if (options.extremeVerbosity) {
      print(toArgv(args));
      print(rc);
      print(`status: ${JSON.stringify(ret[section])}`);
    }
  }

  runTest('testArangoshExitVersion',
          'Starting arangosh printing the version:',
          '',
          0,
          {'version': 'true'});
  print();

  runTest('testArangoshExitCodeConnectAny',
          'Starting arangosh with failing connect:',
          'db._databases();',
          1,
          {'server.endpoint': 'tcp://0.0.0.0:8529'});
  print();

  runTest('testArangoshExitCodeConnectAnyIp6',
          'Starting arangosh with failing connect:',
          'db._databases();',
          1,
          {'server.endpoint': 'tcp://[::]:8529'});
  print();

  runTest('testArangoshExitCodeNoConnect',
          'Starting arangosh with failing connect:',
          'db._databases();',
          1,
          {'server.endpoint': 'tcp://127.0.0.1:0'});
  print();

  runTest('testArangoshExitCodeSyntaxError',
          'Starting arangosh with unparseable script:',
          'tis not js!',
          1,
          {'server.endpoint': 'none'});
  print();

  runTest('testArangoshExitCodeSyntaxErrorInSubScript',
          'Starting arangosh with unparseable script:',
          'let x="tis not js!"; require("internal").executeScript(`${x}`, undefined, "/tmp/1")',
          1,
          {'server.endpoint': 'none'});
  print();

  runTest('testArangoshExitCodeFail',
          'Starting arangosh with exception throwing script:', 'throw(\'foo\')',
          1,
          {'server.endpoint': 'none'});
  print();

  runTest('testArangoshExitCodeFailButCaught', 'Starting arangosh with a caught exception:',
          'try { throw(\'foo\'); } catch (err) {}', 0, {'server.endpoint': 'none'});
  print();

  runTest('testArangoshExitCodeEmpty', 'Starting arangosh with empty script:', '', 0, {'server.endpoint': 'none'});
  print();

  runTest('testArangoshExitCodeSuccess', 'Starting arangosh with regular terminating script:', ';', 0,
          {'server.endpoint': 'none'});
  print();

  runTest('testArangoshExitCodeStatements', 'Starting arangosh with multiple statements:',
          'var a = 1; if (a !== 1) throw("boom!");', 0, {'server.endpoint': 'none'});
  print();

  runTest('testArangoshExitCodeStatements2', 'Starting arangosh with multiple statements:',
          'var a = 1;\nif (a !== 1) throw("boom!");\nif (a === 1) print("success");', 0, {'server.endpoint': 'none'});
  print();

  runTest('testArangoshExitCodeNewlines', 'Starting arangosh with newlines:',
          'q = `FOR i\nIN [1,2,3]\nRETURN i`;\nq += "abc"\n', 0, {'server.endpoint': 'none'});
  print();

  runReadPipeTest('testArangoshReadPipeStalledChild',
                  'readPipe() must honour the deadline while the child is silent:',
                  'stalledChild');
  print();

  runReadPipeTest('testArangoshReadPipeNoEof',
                  'readPipe() must honour the deadline when EOF never arrives:',
                  'noEof');
  print();

  runReadPipeTest('testArangoshReadPipeEofControl',
                  'readPipe() must still terminate on EOF:',
                  'eofControl');
  print();

  runReadPipeTest('testArangoshReadPipeChunking',
                  'readPipe() must keep returning full 1023 byte chunks:',
                  'chunkingControl');
  print();

  runReadPipeTest('testArangoshReadPipeMonitoredProcessDied',
                  'readPipe() must abort when a monitored process dies:',
                  'monitoredProcessDied');
  print();

  runReadPipeTest('testArangoshReadPipeMonitoredProcessExited',
                  'readPipe() must abort when a monitored process exits ' +
                  'cleanly while the child keeps writing:',
                  'monitoredProcessExited');
  print();

  print('\n--------------------------------------------------------------------------------');
  print('pipe through external arangosh');
  print('--------------------------------------------------------------------------------');
  let section = "testArangoshPipeThrough";
  let args = ct.makeArgs.arangosh(options);
  args['javascript.execute-string'] = "print(require('internal').pollStdin())";

  const startTime = time();
  let tmpMgr = new tmpDirMngr('arangosh_tests_pipe', options);
  let sh = new sanHandler(pu.ARANGOSH_BIN, options);
  sh.detectLogfiles(tmpMgr.tempDir, tmpMgr.tempDir);
  let res = executeExternal(pu.ARANGOSH_BIN, toArgv(args), true, sh.getSanOptions());

  let output = '';
  let pipeError = null;
  // fs.readPipe() blocks until it has a full 1023 byte chunk or sees EOF, so a
  // child that never answers would hang the whole suite right here. Arm the
  // execution deadline - the same watchdog the readPipe scenarios above
  // exercise - which takes SECONDS, see client-tools/Shell/v8-deadline.cpp.
  internal.SetGlobalExecutionDeadlineTo(readPipeTimeout);
  try {
    fs.writePipe(res.pid, "bla\n");
    fs.closePipe(res.pid, false);
    output = fs.readPipe(res.pid);
  } catch (ex) {
    pipeError = ex;
  } finally {
    internal.SetGlobalExecutionDeadlineTo(0);
  }
  if (pipeError !== null) {
    // the child is still running, and the statusExternal() below would wait
    // for it indefinitely
    print(RED + 'piping through arangosh failed: ' + pipeError.message + RESET);
    try {
      killExternal(res.pid, 9);
    } catch (ignore) {
    }
  }

  // Arangosh will output a \n on its own, so we will get back 2:
  let searchstring = "bla\n\n";
  let success = (pipeError === null) && (output === searchstring);

  let rc = statusExternal(res.pid, true);
  sh.fetchSanFileAfterExit(res.pid);
  let failSuccess = (rc.hasOwnProperty('exit') && rc.exit === 0);
  failSuccess = failSuccess && success;
  // has to be measured here: the spawn above is not what we are timing
  const deltaTime = time() - startTime;
  if (options.extremeVerbosity) {
      print(toArgv(args));
      print(rc);
      print('pipe output: ' + output);
  }
  if (!failSuccess) {
    ret.failed += 1;
    ret[section] = {
      'failed': 1,
      'message': (pipeError !== null)
        ? 'piping through "bla\\n" failed: ' + pipeError.message
        : 'piping through "bla\\n" didn\'t work out, got: "' + output + '"',
      'total': 1,
      'status': false
    };
  } else {
    ret[section] = {
      'failed': 0,
      'total': 1,
      'status': failSuccess
    };
  }

  ret[section]['duration'] = deltaTime;
  print((failSuccess ? GREEN : RED) + 'Status: ' + (failSuccess ? 'SUCCESS' : 'FAIL') + RESET);

  {
    var echoSuccess = true;
    var deltaTime2 = 0;
    var execFile = fs.getTempFile();

    print('\n--------------------------------------------------------------------------------');
    print('Starting arangosh via echo');
    print('--------------------------------------------------------------------------------');

    fs.write(execFile,
      'echo "db._databases();" | ' + fs.makeAbsolute(pu.ARANGOSH_BIN) + ' --server.endpoint tcp://127.0.0.1:0');

    executeExternalAndWait('sh', ['-c', 'chmod a+x ' + execFile]);

    const startTime2 = time();
    let rc = executeExternalAndWaitWithSanitizer('sh', ['-c', execFile], 'arangosh_tests_echo', options);
    deltaTime2 = time() - startTime2;

    echoSuccess = (rc.hasOwnProperty('exit') && rc.exit === 1);

    if (!echoSuccess) {
      ret.failed += 1;
      ret.testArangoshExitCodeEcho.failed = 1;
      ret.testArangoshExitCodeEcho['message'] =
        'didn\'t get expected return code (1): \n' +
        yaml.safeDump(rc);
    } else {
      ret.testArangoshExitCodeEcho.failed = 0;
    }

    fs.remove(execFile);

    ++ret.testArangoshExitCodeEcho['total'];
    ret.testArangoshExitCodeEcho['status'] = echoSuccess;
    ret.testArangoshExitCodeEcho['duration'] = deltaTime2;
    print((echoSuccess ? GREEN : RED) + 'Status: ' + (echoSuccess ? 'SUCCESS' : 'FAIL') + RESET);
  }

  // test shebang execution with arangosh
  {
    var shebangSuccess = true;
    var deltaTime3 = 0;
    var shebangFile = fs.getTempFile();

    print('\n--------------------------------------------------------------------------------');
    print('Starting arangosh via shebang script');
    print('--------------------------------------------------------------------------------');

    if (options.verbose) {
      print(CYAN + 'shebang script: ' + shebangFile + RESET);
    }

    fs.write(shebangFile,
             '#!' + fs.makeAbsolute(pu.ARANGOSH_BIN) + ' --javascript.execute \n' +
             'print("hello world");\n');

    executeExternalAndWait('sh', ['-c', 'chmod a+x ' + shebangFile]);

    const startTime3 = time();
    rc = executeExternalAndWaitWithSanitizer('sh', ['-c', shebangFile], 'arangosh_tests_shebang', options);
    deltaTime3 = time() - startTime3;

    if (options.verbose) {
      print(CYAN + 'execute returned: ' + RESET, rc);
    }

    shebangSuccess = (rc.hasOwnProperty('exit') && rc.exit === 0);

    if (!shebangSuccess) {
      ret.failed += 1;
      ret.testArangoshShebang.failed = 1;
      ret.testArangoshShebang['message'] =
        'didn\'t get expected return code (0): \n' +
        yaml.safeDump(rc);
    } else {
      ret.testArangoshShebang.failed = 0;
    }
    fs.remove(shebangFile);
  }
  ++ret.testArangoshShebang['total'];
  ret.testArangoshShebang['status'] = shebangSuccess;
  ret.testArangoshShebang['duration'] = deltaTime3;
  print((shebangSuccess ? GREEN : RED) + 'Status: ' + (shebangSuccess ? 'SUCCESS' : 'FAIL') + RESET);
  print();
  return ret;
}

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['arangosh'] = arangosh;

  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
