#!bin/arangosh --javascript.execute
/* jshint globalstrict:false, unused:false */
/* global print, ARGUMENTS */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// Reproduction script for the blocking fs.readPipe().
//
// fs.readPipe() -> JS_ReadPipe() (lib/V8/v8-utils.cpp)
//               -> TRI_ReadPipe() (lib/Basics/process-utils.cpp)
//               -> TRI_ReadPointer() (lib/Basics/files.cpp:757)
//
// TRI_ReadPointer() is a "fill the whole buffer" loop: it returns only once
// 1023 bytes have been collected, or on EOF - i.e. once *every* holder of the
// pipe's write end has closed it - or on a hard error. It even retries on
// EINTR. The pipe itself is created blocking (CreatePipes(), no O_NONBLOCK
// anywhere) and TRI_READ is plain ::read().
//
// While arangosh is parked in that read() no JavaScript runs. Therefore
// neither the global execution deadline nor the ProcessMonitorThread - which
// arms the deadline via triggerV8DeadlineNow() when a monitored process dies -
// can abort the operation. IsDeadlineReached() in the pipe loops of the
// js.js / go.js testsuites is simply never evaluated.
//
// Scenarios A, B and E below hang forever on a build without the
// interruptible read and can only be terminated from the outside; C and D are
// controls which must keep passing on both builds. Use the wrapper
// scripts/repro-readpipe-deadline.sh, which puts every scenario into its own
// arangosh process under a watchdog.
// //////////////////////////////////////////////////////////////////////////////

const fs = require('fs');
const internal = require('internal');

const executeExternal = internal.executeExternal;
const statusExternal = internal.statusExternal;
const killExternal = internal.killExternal;
const addPidToMonitor = internal.addPidToMonitor;
const setDeadline = internal.SetGlobalExecutionDeadlineTo;
const deadlineReason = internal.getDeadlineReasonString;
const time = internal.time;

// the deadline aborts raise TRI_ERROR_DISABLED, see the TRI_CreateErrorObject()
// calls in isExecutionDeadlineReached(), client-tools/Shell/v8-deadline.cpp
const DEADLINE_ERROR_NUM = internal.errors.ERROR_DISABLED.code;

// sizeof(content) - 1 in JS_ReadPipe(), lib/V8/v8-utils.cpp
const CHUNK = 1023;
// the deadline the abort scenarios arm themselves with, in seconds.
// note: SetGlobalExecutionDeadlineTo() takes SECONDS, see
// v8-deadline.cpp (executionDeadline = TRI_microtime() + n)
const DEADLINE = 5;
// how long the children stay silent. Has to outlive the watchdog budget of
// repro-readpipe-deadline.sh, but should not leave long lived strays around.
const STALL = 120;

// `exec` matters: it makes the pid we track the pid that actually holds the
// pipe open and stalls, so killing it later does not orphan a sleeper.
function shell (cmd, usePipes) {
  return executeExternal('/bin/sh', ['-c', cmd], usePipes);
}

function reap (res) {
  if (res === undefined || res === null || res.pid === undefined) {
    return;
  }
  try {
    killExternal(res.pid, 9);
  } catch (ignore) {
  }
  try {
    statusExternal(res.pid, true);
  } catch (ignore) {
  }
}

// Performs one fs.readPipe() and reports how it ended. On a build without the
// interruptible read this call never returns for scenarios A, B and E.
function timedRead (pid) {
  const start = time();
  let result = { buf: null, error: null };
  try {
    result.buf = fs.readPipe(pid);
  } catch (ex) {
    result.error = ex;
  }
  result.elapsed = time() - start;
  return result;
}

// Classifies an expected abort. This has to be strict: getDeadlineReasonString()
// returns its default "Execution deadline reached!" even when no deadline ever
// fired, so matching on the reason alone happily accepts a readPipe() that
// failed for an entirely unrelated cause. We therefore insist on
// TRI_ERROR_DISABLED plus a plausible amount of elapsed time.
function checkAbort (r, expected) {
  const reason = deadlineReason();
  const took = r.elapsed.toFixed(1) + 's';

  if (r.error === null) {
    return {
      ok: false,
      msg: 'readPipe() returned normally after ' + took + ' with ' +
        JSON.stringify(r.buf) + ' - expected the deadline to abort the read'
    };
  }

  const detail = 'after ' + took + ', errorNum ' + r.error.errorNum +
      ', reason "' + reason + '" (' + r.error.message + ')';

  if (r.error.errorNum !== DEADLINE_ERROR_NUM) {
    return {
      ok: false,
      msg: 'readPipe() failed for an unrelated reason ' + detail +
        ' - expected errorNum ' + DEADLINE_ERROR_NUM + ' (ERROR_DISABLED)'
    };
  }
  if (reason.indexOf(expected.reason) === -1) {
    return {
      ok: false,
      msg: 'aborted ' + detail + ' - expected the reason to contain "' +
        expected.reason + '"'
    };
  }
  if (r.elapsed < expected.minElapsed || r.elapsed > expected.maxElapsed) {
    return {
      ok: false,
      msg: 'aborted ' + detail + ' - expected that to take between ' +
        expected.minElapsed + 's and ' + expected.maxElapsed + 's'
    };
  }
  return { ok: true, msg: 'aborted ' + detail };
}

// //////////////////////////////////////////////////////////////////////////////
// A - the js_driver steady state: a child that emitted a few bytes and then
//     went silent without exiting. `mocha --reporter json` behaves exactly
//     like this for the *whole* duration of a run: it buffers everything and
//     prints one JSON blob at the very end, which is why js.js can
//     JSON.parse(allBuff) over the whole accumulation. So arangosh sits in
//     this one read until the run finishes - and oneTestTimeout cannot be
//     enforced while it does.
// //////////////////////////////////////////////////////////////////////////////
function scenarioA () {
  const child = shell('echo hello; exec sleep ' + STALL, true);
  print('  spawned pid ' + child.pid + ', it wrote 6 of the ' + CHUNK +
        ' bytes readPipe() waits for and now stays silent for ' + STALL + 's');
  setDeadline(DEADLINE);
  const r = timedRead(child.pid);
  setDeadline(0);
  const res = checkAbort(r, {
    reason: 'Execution deadline reached',
    minElapsed: DEADLINE - 1,
    maxElapsed: DEADLINE + 5
  });
  reap(child);
  return res;
}

// //////////////////////////////////////////////////////////////////////////////
// B - EOF that can never arrive: the direct child exits at once, but a
//     backgrounded grandchild inherited fd 1 and keeps the write end open, so
//     read() never returns 0 even though the process we track is long gone.
//
//     Do NOT probe with statusExternal() first: TRI_CheckExternalProcess()
//     deletes the ExternalProcess as soon as the status is no longer
//     RUNNING/STOPPED/TIMEOUT, and ~ExternalProcess() closes both pipes. The
//     following readPipe() then fails with "didn't find the process" instead
//     of blocking, which looks like a pass but proves nothing.
//
//     This scenario deliberately leaves the grandchild behind - it is not ours
//     to reap, which is exactly what makes the read unblockable.
// //////////////////////////////////////////////////////////////////////////////
function scenarioB () {
  const child = shell('(sleep ' + STALL + ' &) ; echo bye', true);
  print('  spawned pid ' + child.pid + ' - it exits immediately, but its ' +
        'grandchild keeps fd 1 open, so read() never sees EOF');
  setDeadline(DEADLINE);
  const r = timedRead(child.pid);
  setDeadline(0);
  const res = checkAbort(r, {
    reason: 'Execution deadline reached',
    minElapsed: DEADLINE - 1,
    maxElapsed: DEADLINE + 5
  });
  reap(child);
  return res;
}

// //////////////////////////////////////////////////////////////////////////////
// C - control: a child that writes and exits must still be drained via EOF,
//     with no deadline armed. This is what arangosh.js relies on.
// //////////////////////////////////////////////////////////////////////////////
function scenarioC () {
  const child = shell('echo bye', true);
  const r = timedRead(child.pid);
  reap(child);
  if (r.error !== null) {
    return { ok: false, msg: 'readPipe() threw ' + r.error.message };
  }
  if (r.buf !== 'bye\n') {
    return { ok: false,
             msg: 'expected "bye\\n", got ' + JSON.stringify(r.buf) };
  }
  return {
    ok: true,
    msg: 'read ' + JSON.stringify(r.buf) + ' via EOF in ' +
      r.elapsed.toFixed(2) + 's'
  };
}

// //////////////////////////////////////////////////////////////////////////////
// D - control: chunking must stay byte identical. Every existing caller that
//     loops on `buf.length === 1023` (js.js, go.js, drivers.js, php.js,
//     node-netstat/activators.js) depends on full buffers being returned as
//     full buffers, and on the final short read being delivered. No deadline
//     is armed here, which is the state those callers are usually in.
// //////////////////////////////////////////////////////////////////////////////
function scenarioD () {
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
    return { ok: false, msg: msg + ' - expected ' + total + ' bytes' };
  }
  if (chunks.length < 2 || chunks[0] !== CHUNK || chunks[1] !== CHUNK) {
    return { ok: false,
             msg: msg + ' - expected full ' + CHUNK +
               ' byte chunks until the last one' };
  }
  return { ok: true, msg: msg };
}

// //////////////////////////////////////////////////////////////////////////////
// E - the CI scenario end to end, without needing a cluster: a monitored
//     process (stand-in for a dbserver) dies while we drain a silent child.
//     ProcessMonitorThread::run() notices within ~100ms and arms the deadline
//     through triggerV8DeadlineNow(), which is precisely what the commit
//     "add deadlines to drivers so we can catch dieing server processes"
//     wants to act upon.
// //////////////////////////////////////////////////////////////////////////////
function scenarioE () {
  const victim = shell('exec sleep ' + STALL, false);
  addPidToMonitor(victim.pid);
  const child = shell('echo hello; exec sleep ' + STALL, true);
  print('  monitoring pid ' + victim.pid + ' as a stand-in for a dbserver, ' +
        'draining silent pid ' + child.pid);
  killExternal(victim.pid, 9);
  print('  killed ' + victim.pid + ' - the monitoring thread should arm the ' +
        'deadline within ~100ms');
  const r = timedRead(child.pid);
  setDeadline(0);
  const res = checkAbort(r, {
    reason: 'Monitored child process exited unexpectedly',
    minElapsed: 0,
    maxElapsed: 5
  });
  reap(child);
  reap(victim);
  return res;
}

const SCENARIOS = {
  'A': {
    run: scenarioA,
    expect: 'abort',
    what: 'partial write, then a silent child (the js_driver steady state)'
  },
  'B': {
    run: scenarioB,
    expect: 'abort',
    what: 'child exited, grandchild holds fd 1 open - EOF never arrives'
  },
  'C': {
    run: scenarioC,
    expect: 'complete',
    what: 'control: short read terminated by EOF, no deadline armed'
  },
  'D': {
    run: scenarioD,
    expect: 'complete',
    what: 'control: 1023 byte chunking preserved, no deadline armed'
  },
  'E': {
    run: scenarioE,
    expect: 'abort',
    what: 'monitored process dies while draining a silent child'
  }
};

function main (argv) {
  if (argv.length !== 1 || !SCENARIOS.hasOwnProperty(argv[0])) {
    print('usage: arangosh --javascript.execute ' +
          'scripts/repro-readpipe-deadline.js -- <scenario>');
    print('');
    Object.keys(SCENARIOS).forEach(function (key) {
      print('  ' + key + '  (expects ' + SCENARIOS[key].expect + ')  ' +
            SCENARIOS[key].what);
    });
    return false;
  }

  const key = argv[0];
  const scenario = SCENARIOS[key];
  print('scenario ' + key + ': ' + scenario.what);
  print('  expectation: readPipe() must ' +
        (scenario.expect === 'abort'
          ? 'be aborted within ~' + DEADLINE + 's'
          : 'complete normally') +
        '; without the interruptible read this ' +
        (scenario.expect === 'abort' ? 'hangs forever' : 'passes too'));

  const result = scenario.run();
  print('RESULT ' + key + ' ' + (result.ok ? 'OK' : 'FAIL') + ' ' + result.msg);
  return result.ok;
}

let ok = false;
try {
  ok = main(ARGUMENTS);
} catch (ex) {
  setDeadline(0);
  print('RESULT ' + (ARGUMENTS.length > 0 ? ARGUMENTS[0] : '?') +
        ' FAIL the scenario itself threw: ' + ex.message + '\n' + ex.stack);
}

if (!ok) {
  process.exit(1);
}

