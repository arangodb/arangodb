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

const _ = require('lodash');
const time = require('internal').time;
const fs = require('fs');
const yaml = require('js-yaml');

const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const ct = require('@arangodb/testutils/client-tools');
const internal = require('internal');
const toArgv = internal.toArgv;
const executeScript = internal.executeScript;
const statusExternal = internal.statusExternal;
const executeExternal = internal.executeExternal;
const executeExternalAndWait = internal.executeExternalAndWait;
const tmpDirMngr = require('@arangodb/testutils/tmpDirManager').tmpDirManager;
const {sanHandler} = require('@arangodb/testutils/san-file-handler');

const platform = internal.platform;

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

  function runTest (section, title, command, expectedReturnCode, opts) {
    print('--------------------------------------------------------------------------------');
    print(`testcase ${section} - ${title}`);
    print('--------------------------------------------------------------------------------');

    let weirdNames = ['some dog', 'ла́ять', '犬', 'Kläffer'];
    let tmpMgr = new tmpDirMngr(fs.join('arangosh_tests_weird_names'), options);

    ////////////////////////////////////////////////////////////////////////////////
    // run command from a .js file
    let args = ct.makeArgs.arangosh(options);
    args['javascript.execute-string'] = command;
    args['log.level'] = 'error';

    for (let op in opts) {
      args[op] = opts[op];
    }

    const startTime = time();
    let sh = new sanHandler(pu.ARANGOSH_BIN, options);
    sh.detectLogfiles(tmpMgr.tempDir, tmpMgr.tempDir);
    let rc = executeExternalAndWait(pu.ARANGOSH_BIN, toArgv(args), false, 0, sh.getSanOptions());
    sh.fetchSanFileAfterExit(rc.pid);
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

    let rc2 = executeExternalAndWait(pu.ARANGOSH_BIN, toArgv(args2), false, 0, sh.getSanOptions());
    sh.fetchSanFileAfterExit(rc2.pid);
    const deltaTime2 = time() - startTime;
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
    ret[section]['duration'] = deltaTime;
    print((failSuccess2 ? GREEN : RED) + 'Status: ' + (failSuccess2 ? 'SUCCESS' : 'FAIL') + RESET);
    if (options.extremeVerbosity) {
      print(toArgv(args2));
      print(ret[section]);
      print(rc2);
      print('expect rc: ' + expectedReturnCode);
      print(`status: ${JSON.stringify(ret[section])}`);
    }
    tmpMgr.destructor(ret[section]['status'] && options.cleanup);
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
  let res = executeExternal(pu.ARANGOSH_BIN, toArgv(args), true, 0, sh.getSanOptions());
  const deltaTime = time() - startTime;

  fs.writePipe(res.pid, "bla\n");
  fs.closePipe(res.pid, false);
  let output = fs.readPipe(res.pid);
  // Arangosh will output a \n on its own, so we will get back 2:
  let searchstring = "bla\n\n";
  let success = output === searchstring;

  let rc = statusExternal(res.pid, true);
  sh.fetchSanFileAfterExit(res.pid);
  let failSuccess = (rc.hasOwnProperty('exit') && rc.exit === 0);
  failSuccess = failSuccess && success;
  if (options.extremeVerbosity) {
      print(toArgv(args));
      print(rc);
      print('pipe output: ' + output);
  }
  if (!failSuccess) {
    ret.failed += 1;
    ret[section] = {
      'failed': 1,
      'message': 'piping through "bla\\n" didn\'t work out, got: "' +
        output + '"',
      'total': 1
    };
  } else {
    ret[section] = {
      'failed': 0,
      'total': 1,
      'status': failSuccess
    };
  }

  ret[section]['duration'] = time() - startTime;
  print((failSuccess ? GREEN : RED) + 'Status: ' + (failSuccess ? 'SUCCESS' : 'FAIL') + RESET);

  {
    let tmpMgr = new tmpDirMngr('arangosh_tests_echo', options);
    var echoSuccess = true;
    var deltaTime2 = 0;
    var execFile = fs.getTempFile();

    print('\n--------------------------------------------------------------------------------');
    print('Starting arangosh via echo');
    print('--------------------------------------------------------------------------------');

    fs.write(execFile,
      'echo "db._databases();" | ' + fs.makeAbsolute(pu.ARANGOSH_BIN) + ' --server.endpoint tcp://127.0.0.1:0');

    let sh = new sanHandler(pu.ARANGOSH_BIN, options);
    sh.detectLogfiles(tmpMgr.tempDir, tmpMgr.tempDir);
    executeExternalAndWait('sh', ['-c', 'chmod a+x ' + execFile]);

    const startTime2 = time();
    let rc = executeExternalAndWait('sh', ['-c', execFile], false, 0, sh.getSanOptions());
    sh.fetchSanFileAfterExit(rc.pid);
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
    let tmpMgr = new tmpDirMngr('arangosh_tests_shebang', options);
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
    sh.detectLogfiles(tmpMgr.tempDir, tmpMgr.tempDir);
    rc = executeExternalAndWait('sh', ['-c', shebangFile], false, sh.getSanOptions());
    sh.fetchSanFileAfterExit(rc.pid);
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
