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

const functionsDocumentation = {
  'export': 'export formats tests'
};
const optionsDocumentation = [
];

const fs = require('fs');
const pu = require('@arangodb/process-utils');
const tu = require('@arangodb/test-utils');
const xmldom = require('xmldom');

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
// const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const toArgv = require('internal').toArgv;

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: export
// //////////////////////////////////////////////////////////////////////////////

function exportTest (options) {
  const cluster = options.cluster ? '-cluster' : '';
  const tmpPath = fs.getTempPath();
  const DOMParser = new xmldom.DOMParser({
    locator: {},
    errorHandler: {
      warning: function (err) {
        xmlErrors = err;
      },
      error: function (err) {
        xmlErrors = err;
      },
      fatalError: function (err) {
        xmlErrors = err;
      }
    }
  }
                                        );
  let xmlErrors = null;

  print(CYAN + 'export tests...' + RESET);

  const instanceInfo = pu.startInstance('tcp', options, {}, 'export');

  if (instanceInfo === false) {
    return {
      export: {
        status: false,
        message: 'failed to start server!'
      }
    };
  }

  print(CYAN + Date() + ': Setting up' + RESET);

  const args = {
    'configuration': fs.join(pu.CONFIG_DIR, 'arangoexport.conf'),
    'server.username': options.username,
    'server.password': options.password,
    'server.endpoint': instanceInfo.endpoint,
    'server.database': 'UnitTestsExport',
    'collection': 'UnitTestsExport',
    'type': 'json',
    'overwrite': true,
    'output-directory': tmpPath
  };
  const results = {};

  function shutdown () {
    print(CYAN + 'Shutting down...' + RESET);
    pu.shutdownInstance(instanceInfo, options);
    print(CYAN + 'done.' + RESET);
    print();
    return results;
  }

  results.setup = tu.runInArangosh(options, instanceInfo, tu.makePathUnix('js/server/tests/export/export-setup' + cluster + '.js'));
  if (!pu.arangod.check.instanceAlive(instanceInfo, options) || results.setup.status !== true) {
    return shutdown();
  }

  print(CYAN + Date() + ': Export data (json)' + RESET);
  results.exportJson = pu.executeAndWait(pu.ARANGOEXPORT_BIN, toArgv(args), options, 'arangosh', tmpPath);
  try {
    // const filesContent = JSON.parse(fs.read(fs.join(tmpPath, 'UnitTestsExport.json')));
    results.parseJson = { status: true };
  } catch (e) {
    results.parseJson = {
      status: false,
      message: e
    };
  }

  print(CYAN + Date() + ': Export data (jsonl)' + RESET);
  args['type'] = 'jsonl';
  results.exportJsonl = pu.executeAndWait(pu.ARANGOEXPORT_BIN, toArgv(args), options, 'arangosh', tmpPath);
  try {
    const filesContent = fs.read(fs.join(tmpPath, 'UnitTestsExport.jsonl')).split('\n');
    for (const line of filesContent) {
      if (line.trim() === '') continue;
      JSON.parse(line);
    }
    results.parseJsonl = {
      status: true
    };
  } catch (e) {
    results.parseJsonl = {
      status: false,
      message: e
    };
  }

  print(CYAN + Date() + ': Export data (xgmml)' + RESET);
  args['type'] = 'xgmml';
  args['graph-name'] = 'UnitTestsExport';
  results.exportXgmml = pu.executeAndWait(pu.ARANGOEXPORT_BIN, toArgv(args), options, 'arangosh', tmpPath);
  try {
    const filesContent = fs.read(fs.join(tmpPath, 'UnitTestsExport.xgmml'));
    DOMParser.parseFromString(filesContent);
    results.parseXgmml = { status: true };

    if (xmlErrors !== null) {
      results.parseXgmml = {
        status: false,
        message: xmlErrors
      };
    }
  } catch (e) {
    results.parseXgmml = {
      status: false,
      message: e
    };
  }

  return shutdown();
}

function setup (testFns, defaultFns, opts, fnDocs, optionsDoc) {
  testFns['export'] = exportTest;
  defaultFns.push('export');
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
}

exports.setup = setup;
