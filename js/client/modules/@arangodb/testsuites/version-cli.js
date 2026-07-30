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

function record(results, key, ok, message) {
  results[key] = ok
    ? {failed: 0, status: true, total: 1}
    : {failed: 1, status: false, total: 1, message: message};
  if (!ok) {
    results.failed += 1;
  }
}

function versionPrinted(output, expected) {
  return output.split('\n')[0] === expected;
}

function versionJsonPrinted(output, expected) {
  try {
    return JSON.parse(output).version === expected;
  } catch (e) {
    return false;
  }
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
      const ok = rc.exit === 0 && versionPrinted(output, expected);
      record(results, key, ok, `exit=${JSON.stringify(rc)} output=${output}`);
    }

    {
      const key = `${binary}--version-json`;
      print(`checking ${binary} --version-json`);
      const {output, rc} = runCommand(binaryPath, ['--version-json'], options, rootDir);
      const ok = rc.exit === 0 && versionJsonPrinted(output, expected);
      record(results, key, ok, `exit=${JSON.stringify(rc)} output=${output}`);
    }
  }

  // Boolean / '=' forms: one binary is enough
  const probe = fs.join(pu.BIN_DIR, 'arangod' + pu.executableExt);
  if (fs.exists(probe)) {
    const printCases = [
      ['--version', 'true'],
      ['--version=true'],
      ['--version', 'abc'],
      ['--version-json', 'true'],
      ['--version-json=true']
    ];
    for (const args of printCases) {
      const key = `arangod${args.join('')}`;
      print(`checking arangod ${args.join(' ')}`);
      const {output, rc} = runCommand(probe, args, options, rootDir);
      const wantJson = args[0].startsWith('--version-json');
      const ok = rc.exit === 0 &&
        (wantJson ? versionJsonPrinted(output, expected)
                  : versionPrinted(output, expected));
      record(results, key, ok, `exit=${JSON.stringify(rc)} output=${output}`);
    }

    const noPrintCases = [
      ['--version', 'false'],
      ['--version=false'],
      ['--version-json', 'false'],
      ['--version-json=false']
    ];
    for (const args of noPrintCases) {
      const key = `arangod${args.join('')}`;
      print(`checking arangod ${args.join(' ')}`);
      const {output, rc} = runCommand(probe, args, options, rootDir);
      const wantJson = args[0].startsWith('--version-json');
      const printed = wantJson ? versionJsonPrinted(output, expected)
                               : versionPrinted(output, expected);
      record(results, key, !printed,
             `version must not be printed; exit=${JSON.stringify(rc)} output=${output}`);
    }

    const invalidCases = [
      ['--version=abc'],
      ['--version-json=abc']
    ];
    for (const args of invalidCases) {
      const key = `arangod${args.join('')}`;
      print(`checking arangod ${args.join(' ')}`);
      const {output, rc} = runCommand(probe, args, options, rootDir);
      const printed = versionPrinted(output, expected) ||
                      versionJsonPrinted(output, expected);
      const ok = rc.exit !== 0 && !printed;
      record(results, key, ok,
             `expected parse failure without version output; exit=${JSON.stringify(rc)} output=${output}`);
    }

    {
      const key = 'arangod--help-lists-version';
      print('checking arangod --help lists --version');
      const {output, rc} = runCommand(probe, ['--help'], options, rootDir);
      const ok = rc.exit === 0 &&
        output.indexOf('--version') !== -1 &&
        output.indexOf('--version-json') !== -1;
      record(results, key, ok, `exit=${JSON.stringify(rc)} output=${output}`);
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
