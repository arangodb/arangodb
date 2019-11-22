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
const optionsDocumentation = [];

const fs = require('fs');
const pu = require('@arangodb/process-utils');
const tu = require('@arangodb/test-utils');
const xmldom = require('xmldom');
const zlib = require('zlib');

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
// const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const toArgv = require('internal').toArgv;

const testPaths = {
  'export': [tu.pathForTesting('server/export')] // we have to be fuzzy...
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: export
// //////////////////////////////////////////////////////////////////////////////

function exportTest (options) {
  const cluster = options.cluster ? '-cluster' : '';
  const tmpPath = fs.join(options.testOutputDirectory, 'export');
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
  let results = {failed: 0};

  function shutdown () {
    print(CYAN + 'Shutting down...' + RESET);
    results['shutdown'] = pu.shutdownInstance(instanceInfo, options);
    print(CYAN + 'done.' + RESET);
    print();
    return results;
  }

  results.setup = tu.runInArangosh(options, instanceInfo, tu.makePathUnix(tu.pathForTesting('server/export/export-setup' + cluster + '.js')));
  results.setup.failed = 0;
  if (!pu.arangod.check.instanceAlive(instanceInfo, options) || results.setup.status !== true) {
    results.setup.failed = 1;
    results.failed += 1;
    return shutdown();
  }

  let skipEncrypt = true;
  let keyfile = "";
  if (global.ARANGODB_CLIENT_VERSION) {
    let version = global.ARANGODB_CLIENT_VERSION(true);
    if (version.hasOwnProperty('enterprise-version')) {
      skipEncrypt = false;
      keyfile = fs.join(instanceInfo.rootDir, 'secret-key');
      fs.write(keyfile, 'DER-HUND-der-hund-der-hund-der-h'); // must be exactly 32 chars long
    }
  }

  print(CYAN + Date() + ': Export data (json)' + RESET);
  results.exportJson = pu.executeAndWait(pu.ARANGOEXPORT_BIN, toArgv(args), options, 'arangosh', tmpPath, options.coreCheck);
  results.exportJson.failed = results.exportJson.status ? 0 : 1;

  try {
    JSON.parse(fs.read(fs.join(tmpPath, 'UnitTestsExport.json')));
    results.parseJson = {
      failed: 0,
      status: true
    };
  } catch (e) {
    results.failed += 1;
    results.parseJson = {
      failed: 1,
      status: false,
      message: e
    };
  }

  print(CYAN + Date() + ': Export data (json.gz)' + RESET);
  args['compress-output'] = 'true';
  results.exportJsonGz = pu.executeAndWait(pu.ARANGOEXPORT_BIN, toArgv(args), options, 'arangosh', tmpPath, options.coreCheck);
  results.exportJsonGz.failed = results.exportJsonGz.status ? 0 : 1;

  try {
    const zipBuffer = fs.readGzip(fs.join(tmpPath, 'UnitTestsExport.json.gz'));
    JSON.parse(zipBuffer);
    results.parseJsonGz = {
      failed: 0,
      status: true
    };
  } catch (e) {
    results.failed += 1;
    results.parseJsonGz = {
      failed: 1,
      status: false,
      message: e
    };
  }
  args['compress-output'] = 'false';

  if (!skipEncrypt) {
    print(CYAN + Date() + ': Export data (json encrypt)' + RESET);
    args['encryption.keyfile'] = keyfile;
    if (fs.exists(fs.join(tmpPath, 'ENCRYPTION'))) {
      fs.remove(fs.join(tmpPath, 'ENCRYPTION'));
    }
    results.exportJsonEncrypt = pu.executeAndWait(pu.ARANGOEXPORT_BIN, toArgv(args), options, 'arangosh', tmpPath, options.coreCheck);
    results.exportJsonEncrypt.failed = results.exportJsonGz.status ? 0 : 1;

    try {
      const decBuffer = fs.readDecrypt(fs.join(tmpPath, 'UnitTestsExport.json'), keyfile);
      JSON.parse(decBuffer);
      results.parseJsonEncrypt = {
        failed: 0,
        status: true
      };
    } catch (e) {
      results.failed += 1;
      results.parseJsonEncrypt = {
        failed: 1,
        status: false,
        message: e
      };
    }
    delete args['encryption.keyfile'];
    if (fs.exists(fs.join(tmpPath, 'ENCRYPTION'))) {
      fs.remove(fs.join(tmpPath, 'ENCRYPTION'));
    }
  }

  print(CYAN + Date() + ': Export data (jsonl)' + RESET);
  args['type'] = 'jsonl';
  results.exportJsonl = pu.executeAndWait(pu.ARANGOEXPORT_BIN, toArgv(args), options, 'arangosh', tmpPath, options.coreCheck);
  results.exportJsonl.failed = results.exportJsonl.status ? 0 : 1;
  try {
    fs.read(fs.join(tmpPath, 'UnitTestsExport.jsonl')).split('\n')
    .filter(line => line.trim() !== '')
    .forEach(line => JSON.parse(line));

    results.parseJsonl = {
      failed: 0,
      status: true
    };
  } catch (e) {
    results.failed += 1;
    results.parseJsonl = {
      failed: 1,
      status: false,
      message: e
    };
  }

  print(CYAN + Date() + ': Export data (jsonl.gz)' + RESET);
  args['compress-output'] = 'true';
  results.exportJsonlGz = pu.executeAndWait(pu.ARANGOEXPORT_BIN, toArgv(args), options, 'arangosh', tmpPath, options.coreCheck);
  results.exportJsonlGz.failed = results.exportJsonl.status ? 0 : 1;
  try {
    fs.readGzip(fs.join(tmpPath, 'UnitTestsExport.jsonl.gz')).split('\n')
    .filter(line => line.trim() !== '')
    .forEach(line => JSON.parse(line));

    results.parseJsonlGz = {
      failed: 0,
      status: true
    };
  } catch (e) {
    results.failed += 1;
    results.parseJsonlGz = {
      failed: 1,
      status: false,
      message: e
    };
  }
  args['compress-output'] = 'false';

  print(CYAN + Date() + ': Export data (xgmml)' + RESET);
  args['type'] = 'xgmml';
  args['graph-name'] = 'UnitTestsExport';
  results.exportXgmml = pu.executeAndWait(pu.ARANGOEXPORT_BIN, toArgv(args), options, 'arangosh', tmpPath, options.coreCheck);
  results.exportXgmml.failed = results.exportXgmml.status ? 0 : 1;
  try {
    const filesContent = fs.read(fs.join(tmpPath, 'UnitTestsExport.xgmml'));
    DOMParser.parseFromString(filesContent);
    results.parseXgmml = {
      failed: 0,
      status: true
    };

    if (xmlErrors !== null) {
      results.parseXgmml = {
        failed: 1,
        status: false,
        message: xmlErrors
      };
    }
  } catch (e) {
    results.failed += 1;
    results.parseXgmml = {
      failed: 1,
      status: false,
      message: e
    };
  }

  print(CYAN + Date() + ': Export data (xgmml.gz)' + RESET);
  args['compress-output'] = 'true';
  results.exportXgmmlGz = pu.executeAndWait(pu.ARANGOEXPORT_BIN, toArgv(args), options, 'arangosh', tmpPath, options.coreCheck);
  results.exportXgmmlGz.failed = results.exportXgmml.status ? 0 : 1;
  try {
    const filesContent = fs.readGzip(fs.join(tmpPath, 'UnitTestsExport.xgmml.gz'));
    DOMParser.parseFromString(filesContent);
    results.parseXgmmlGz = {
      failed: 0,
      status: true
    };

    if (xmlErrors !== null) {
      results.parseXgmmlGz = {
        failed: 1,
        status: false,
        message: xmlErrors
      };
    }
  } catch (e) {
    results.failed += 1;
    results.parseXgmmlGz = {
      failed: 1,
      status: false,
      message: e
    };
  }
  args['compress-output'] = 'false';

  print(CYAN + Date() + ': Export query (xgmml)' + RESET);
  args['type'] = 'jsonl';
  args['query'] = 'FOR doc IN UnitTestsExport RETURN doc';
  delete args['graph-name'];
  delete args['collection'];
  results.exportQuery = pu.executeAndWait(pu.ARANGOEXPORT_BIN, toArgv(args), options, 'arangosh', tmpPath, options.coreCheck);
  results.exportQuery.failed = results.exportQuery.status ? 0 : 1;
  try {
    fs.read(fs.join(tmpPath, 'query.jsonl')).split('\n')
    .filter(line => line.trim() !== '')
    .forEach(line => JSON.parse(line));
    results.parseQueryResult = {
      failed: 0,
      status: true
    };
  } catch (e) {
    print(e);
    results.failed += 1;
    results.parseQueryResult = {
      failed: 1,
      status: false,
      message: e
    };
  }

  print(CYAN + Date() + ': Export query (xgmml.gz)' + RESET);
  args['compress-output'] = 'true';
  results.exportQueryGz = pu.executeAndWait(pu.ARANGOEXPORT_BIN, toArgv(args), options, 'arangosh', tmpPath, options.coreCheck);
  results.exportQueryGz.failed = results.exportQuery.status ? 0 : 1;
  try {
    fs.readGzip(fs.join(tmpPath, 'query.jsonl.gz')).split('\n')
    .filter(line => line.trim() !== '')
    .forEach(line => JSON.parse(line));
    results.parseQueryResultGz = {
      failed: 0,
      status: true
    };
  } catch (e) {
    results.failed += 1;
    results.parseQueryResultGz = {
      failed: 1,
      status: false,
      message: e
    };
  }
  args['compress-output'] = 'false';
  
  print(CYAN + Date() + ': Export data (csv)' + RESET);
  args['type'] = 'csv';
  args['query'] = 'FOR doc IN UnitTestsExport RETURN doc';
  args['fields'] = '_key,value1,value2,value3,value4';
  results.exportCsv = pu.executeAndWait(pu.ARANGOEXPORT_BIN, toArgv(args), options, 'arangosh', tmpPath, false, options.coreCheck);
  results.exportCsv.failed = results.exportJsonl.status ? 0 : 1;
  try {
    fs.read(fs.join(tmpPath, 'query.csv'));

    results.parseCsv = {
      failed: 0,
      status: true
    };
  } catch (e) {
    results.failed += 1;
    results.parseCsv = {
      failed: 1,
      status: false,
      message: e
    };
  }
  delete args['fields'];

  return shutdown();
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['export'] = exportTest;
  defaultFns.push('export');
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
