/* jshint strict: false, sub: true */
/* global print */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
/// Copyright 2014 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const fs = require('fs');
const pu = require('@arangodb/process-utils');
const tu = require('@arangodb/test-utils');
const toArgv = require('internal').toArgv;

const optionsDocumentation = [
  '   - `upgradeDataPath`: path to directory containing upgrade data archives'
];

const compareSemVer = (a, b) => {
  if (a === b) {
    return 0;
  }
  let lex = false;
  const partsA = a.split('-')[0].split('.');
  const partsB = b.split('-')[0].split('.');
  if (partsA.length < 2 ||
    partsA.length > 4 ||
    partsB.length < 2 ||
    partsB.length > 4 ||
    !partsA.every(p => isNaN(Number(p))) ||
    !partsB.every(p => isNaN(Number(b)))) {
    return (a < b) ? -1 : 1;
  }

  for (let i = partsA.length; i < 4; i++) {
    partsA.push_back("0");
  }
  for (let i = partsB.length; i < 4; i++) {
    partsB.push_back("0");
  }

  for (let i = 0; i < 4; i++) {
    const numA = Number(partsA[i]);
    const numB = Number(partsB[i]);
    if (numA < numB) {
      return -1;
    }
    if (numB < numA) {
      return 1;
    }
  }
};

const byMinimumSuportedVersion = (version) => {
  return (testCase) => {
    let supported = true;
    testCase.substring(0, testCase.length - 3).split('-').map((s) => {
      if (s.startsWith("msv")) {
        const msv = s.substring(3);
        if (compareSemVer(msv, version) > 0) {
          supported = false;
        }
      }
    });
    return supported;
  };
};

////////////////////////////////////////////////////////////////////////////////
/// set up the test according to the testcase.
////////////////////////////////////////////////////////////////////////////////
const unpackOldData = (engine, version, options, serverOptions) => {
  const archiveName = `upgrade-data-${version}-${engine}`;
  const dataFile = fs.join(options.upgradeDataPath,
    'data',
    `${archiveName}.tar.gz`);
  const tarOptions = [
    '--extract',
    '--gunzip',
    `--file=${dataFile}`,
    `--directory=${serverOptions['database.directory']}`
  ];
  let unpack = pu.executeAndWait('tar', tarOptions, { disableMonitor: true }, '');

  if (unpack.status === false) {
    unpack.failed = 1;
    return {
      status: false,
      message: unpack.message
    };
  }

  return {
    status: true
  };
};

////////////////////////////////////////////////////////////////////////////////
/// testcases themselves
////////////////////////////////////////////////////////////////////////////////

const upgradeData = (engine, version) => {
  return (options) => {
    const testName = `upgrade_data_${version}_${engine}`;
    const dataFile = fs.join(options.upgradeDataPath,
      'data',
      `upgrade-data-${version}-${engine}.tar.gz`);
    if (options.storageEngine !== engine) {
      // engine mismatch, skip!
      const res = {};
      res[testName] = {
        failed: 0,
        'status': true,
        'message': 'skipped because of engine mismatch',
        'skipped': true
      };
      return res;
    } else if (!fs.exists(dataFile)) {
      // data file missing
      const res = {};
      if (options.upgradeDataMissingShouldFail) {
        res[testName] = {
          failed: 1,
          'status': false,
          'message': `failed due to missing data file ${dataFile}`,
          'skipped': false
        };
      } else {
        res[testName] = {
          failed: 0,
          'status': true,
          'message': `skipped due to missing data file ${dataFile}`,
          'skipped': true
        };
      }
      return res;
    }

    const tmpDataDir = fs.join(fs.getTempPath(), testName);
    fs.makeDirectoryRecursive(tmpDataDir);
    pu.cleanupDBDirectoriesAppend(tmpDataDir);

    const appDir = fs.join(tmpDataDir, 'apps');
    fs.makeDirectoryRecursive(appDir);

    const tmpDir = fs.join(tmpDataDir, 'tmp');
    fs.makeDirectoryRecursive(tmpDir);

    const dataDir = fs.join(tmpDataDir, 'data');
    fs.makeDirectoryRecursive(dataDir);

    const port = pu.findFreePort(options.minPort, options.maxPort);

    let args = pu.makeArgs.arangod(options, appDir, '', tmpDir);
    args['server.endpoint'] = 'tcp://127.0.0.1:' + port;
    args['database.directory'] = dataDir;
    args['database.auto-upgrade'] = true;

    require('internal').print('Unpacking old data...');
    let result = unpackOldData(engine, version, options, args);
    if (result.status !== true) {
      return result;
    }

    require('internal').print('Running upgrade...');
    const argv = toArgv(args);
    result = pu.executeAndWait(pu.ARANGOD_BIN, argv, options, 'upgrade', tmpDataDir);
    if (result.status !== true) {
      return {
        failed: 1,
        'status': false,
        'message': 'upgrade result: ' + result.message,
        'skipped': false
      };
    }

    args['database.auto-upgrade'] = false;

    const testCases = tu.scanTestPaths(['tests/js/server/upgrade-data'], options)
      .filter(byMinimumSuportedVersion(version));
    require('internal').print('Checking results...');
    return tu.performTests(
      options,
      testCases,
      `upgrade_data_${engine}_${version}`,
      tu.runThere,
      args);
  };
};

exports.setup = function(testFns, defaultFns, opts, fnDocs, optionsDoc) {
  const functionsDocumentation = {};
  const configurations = fs.list('upgrade-data-tests/data').map(
    (filename) => {
      const re = /upgrade-data-(\d+(?:\.\d+)*(-\d)?)-(rocksdb|mmfiles)\.tar\.gz/;
      const matches = re.exec(filename);
      return {
        engine: matches[3],
        version: matches[1]
      };
    }
  ).filter((f) => {
    return f.engine === 'rocksdb';
  }).sort((a, b) => {
    const versionDiff = compareSemVer(a.version, b.version);
    if (versionDiff !== 0) {
      return versionDiff;
    }
    if (a.engine < b.engine) return -1;
    if (a.engine > b.engine) return 1;
    return 0;
  });

  for (let i = 0; i < configurations.length; i++) {
    const {
      engine,
      version
    } = configurations[i];
    const testName = `upgrade_data_${version}_${engine}`;
      
    testFns[testName] = upgradeData(engine, version);
    defaultFns.push(testName);
    functionsDocumentation[testName] =
      `test upgrade from version ${version} using ${engine} engine`;
  }

  opts['upgradeDataPath'] = 'upgrade-data-tests';
  opts['upgradeDataMissingShouldFail'] = false;

  /* jshint forin: false */
  for (var attrname in functionsDocumentation) {
    fnDocs[attrname] = functionsDocumentation[attrname];
  }
  for (var i = 0; i < optionsDocumentation.length; i++) {
    optionsDoc.push(optionsDocumentation[i]);
  }
};
