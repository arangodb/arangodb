/* jshint strict: false, sub: true */
/* global print */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'version-cli': 'checks --version and --version-json'
};

const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const internal = require('internal');
const executeExternal = internal.executeExternal;
const statusExternal = internal.statusExternal;
const {sanHandler} = require('@arangodb/testutils/san-file-handler');

const testPaths = {
  'version-cli': []
};

const binaries = [
  'arangod',
  'arangosh',
  'arangodump',
  'arangoimport',
  'arangorestore',
  'arangoexport',
  'arangobench',
  'arangobackup',
  'arangovpack'
];

function runCommand(binaryPath, args, options, rootDir) {
  const sh = new sanHandler(binaryPath, options);
  sh.detectLogfiles(rootDir, rootDir);
  const res = executeExternal(binaryPath, args, true, sh.getSanOptions());

  let output = '';
  let buf;
  do {
    buf = fs.readPipe(res.pid);
    output += buf;
  } while (buf.length > 0);

  const rc = statusExternal(res.pid, true);
  sh.fetchSanFileAfterExit(res.pid);
  return {output, rc};
}

function checkResult(name, expected, got, ok) {
  if (ok) {
    return {failed: 0, status: true, total: 1};
  }
  return {
    failed: 1,
    status: false,
    total: 1,
    message: `${name}: expected '${expected}', got '${got}'`
  };
}

function versionCli(options) {
  const expected = internal.version;
  const rootDir = fs.join(fs.getTempPath(), 'version-cli');
  fs.makeDirectoryRecursive(rootDir);

  let results = {failed: 0};

  for (const binary of binaries) {
    const binaryPath = fs.join(pu.BIN_DIR, binary + pu.executableExt);
    if (!fs.exists(binaryPath)) {
      continue;
    }

    {
      const key = `${binary}--version`;
      print(`checking ${binary} --version`);
      const {output, rc} = runCommand(binaryPath, ['--version'], options, rootDir);
      const firstLine = output.split('\n')[0];
      const ok = rc.hasOwnProperty('exit') && rc.exit === 0 && firstLine === expected;
      results[key] = checkResult(key, expected, `exit=${JSON.stringify(rc)} output=${output}`, ok);
      if (!results[key].status) {
        results.failed += 1;
      }
    }

    {
      const key = `${binary}--version-json`;
      print(`checking ${binary} --version-json`);
      const {output, rc} = runCommand(binaryPath, ['--version-json'], options, rootDir);
      let version = null;
      try {
        version = JSON.parse(output).version;
      } catch (e) {
        version = `JSON parse error: ${e.message}; output=${output}`;
      }
      const ok = rc.hasOwnProperty('exit') && rc.exit === 0 && version === expected;
      results[key] = checkResult(key, expected, `exit=${JSON.stringify(rc)} version=${version}`, ok);
      if (!results[key].status) {
        results.failed += 1;
      }
    }
  }

  print();
  return results;
}

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['version-cli'] = versionCli;
  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
