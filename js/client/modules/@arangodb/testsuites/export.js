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

const functionsDocumentation = {
  'export': 'export formats tests'
};

const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const trs = require('@arangodb/testutils/testrunners');
const im = require('@arangodb/testutils/instance-manager');
const xmldom = require('@xmldom/xmldom');
const zlib = require('zlib');
const internal = require('internal');
const time = internal.time;

const CYAN = internal.COLORS.COLOR_CYAN;
const RESET = internal.COLORS.COLOR_RESET;
const GREEN = internal.COLORS.COLOR_GREEN;

const toArgv = internal.toArgv;
const isEnterprise = require("@arangodb/test-helper").isEnterprise;

const testPaths = {
  'export': [tu.pathForTesting('client/export')] // we have to be fuzzy...
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: export
// //////////////////////////////////////////////////////////////////////////////
class exportRunner extends trs.runInArangoshRunner {
  constructor(options, testname, ...optionalArgs) {
    super(options, testname, ...optionalArgs);
    this.info = "runExport";
    this.instanceManage = null;
  }
  
  shutdown (results) {
    print(CYAN + 'Shutting down...' + RESET);
    results['shutdown'] = this.instanceManager.shutdownInstance(false);
    results.status = results.failed === 0 && results['shutdown'];
    this.instanceManager.destructor(results.status);
    print(CYAN + 'done.' + RESET);
    print();
    return results;
  }

  run() {
    const timeout = 60;
    const tmpPath = fs.join(this.options.testOutputDirectory, 'export');
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
    });
    let xmlErrors = null;

    print(CYAN + 'export tests...' + RESET);

    this.instanceManager = new im.instanceManager('tcp', this.options, {}, 'export');
    this.instanceManager.prepareInstance();
    this.instanceManager.launchTcpDump("");
    if (!this.instanceManager.launchInstance()) {
      return {
        export: {
          status: false,
          message: 'failed to start server!'
        }
      };
    }
    this.instanceManager.reconnect();

    print(CYAN + Date() + ': Setting up' + RESET);

    let results = {failed: 0};

    results.setup = this.runOneTest(tu.makePathUnix(tu.pathForTesting('client/export/export-setup.js')));
    results.setup.failed = 0;
    if (!this.instanceManager.checkInstanceAlive() || results.setup.status !== true) {
      results.setup.failed = 1;
      results.failed += 1;
      return this.shutdown(results);
    }
    let queryFile = fs.join(tmpPath, 'query-string');
    fs.makeDirectoryRecursive(tmpPath);
    fs.writeFileSync(queryFile, 'FOR doc IN UnitTestsExport RETURN doc');

    let skipEncrypt = true;
    let keyfile = "";
    if (isEnterprise()) {
      skipEncrypt = false;
      keyfile = fs.join(this.instanceManager.rootDir, 'secret-key');
      fs.write(keyfile, 'DER-HUND-der-hund-der-hund-der-h'); // must be exactly 32 chars long
    }
    
    let databases = [
      "UnitTestsExport",
      "اسم قاعدة بيانات يونيكود",
      "имя базы данных юникода !\"23 ää"
    ];

    // loop over different databases
    databases.forEach((name, idx) => {
      try {
        fs.removeDirectory(tmpPath);
      } catch (err) {
      }

      const commonArgValues = {
        'configuration': fs.join(pu.CONFIG_DIR, 'arangoexport.conf'),
        'server.username': this.options.username,
        'server.password': this.options.password,
        'server.endpoint': this.instanceManager.endpoint,
        'server.database': name,
        'overwrite': true,
        'output-directory': tmpPath
      };
      let testParseJSONFile = function() {
        let tstFile = fs.join(tmpPath, 'UnitTestsExport.json');
        try {
          JSON.parse(fs.read(tstFile));
        } finally {
          if (fs.exists(tstFile)) { fs.remove(tstFile);}
        }
      };
      let testParseJSONZipFile = function() {
         let tstFile = fs.join(tmpPath, 'UnitTestsExport.json.gz');
        try {
          const zipBuffer = fs.readGzip(tstFile);
          JSON.parse(zipBuffer);
        } finally {
          if (fs.exists(tstFile)) { fs.remove(tstFile);}
        }
      };
      let testParseJSONlFile = function() {
        let tstFile = fs.join(tmpPath, 'UnitTestsExport.jsonl');
        try {
          fs.read(tstFile).split('\n')
            .filter(line => line.trim() !== '')
            .forEach(line => JSON.parse(line));
        } finally {
          if (fs.exists(tstFile)) { fs.remove(tstFile);}
        }
      };
      let _testParseJSONlZipFile = function(fn) {
        let tstFile = fs.join(tmpPath, fn);
        try {
        fs.readGzip(tstFile).split('\n')
          .filter(line => line.trim() !== '')
          .forEach(line => JSON.parse(line));
        } finally {
          if (fs.exists(tstFile)) { fs.remove(tstFile);}
        }
      };
      let testParseJSONlZipFile = function() { _testParseJSONlZipFile('UnitTestsExport.jsonl.gz'); };
      let testParseQueryJSONlZipFile = function() {_testParseJSONlZipFile('query.jsonl.gz'); };
      let testParseQueryJSONlFile = function() {_testParseJSONlZipFile('query.jsonl'); };
      let testParseXGGMLFile = function() {
        let tstFile = fs.join(tmpPath, 'UnitTestsExport.xgmml');
        try {
        const filesContent = fs.read(tstFile);
        DOMParser.parseFromString(filesContent);
        } finally {
          if (fs.exists(tstFile)) { fs.remove(tstFile);}
        }
      };
      let testParseXGGMLZipFile = function() {
        let tstFile = fs.join(tmpPath, 'UnitTestsExport.xgmml.gz');
        try {
        const filesContent = fs.readGzip(tstFile);
        DOMParser.parseFromString(filesContent);
        } finally {
          if (fs.exists(tstFile)) { fs.remove(tstFile);}
        }
      };
      let testReadCSVFile = function() {
        let tstFile = fs.join(tmpPath, 'query.csv');
        try {
          let content = fs.read(tstFile);
        } finally {
          if (fs.exists(tstFile)) { fs.remove(tstFile);}
        }
      };
      let exportTestCases = [
        {
          "name": "exportJson" + idx,
          "args": {
            'collection': 'UnitTestsExport',
            'type': 'json'
          },
          "validate": testParseJSONFile
        }, {
          "name": "exportJsonGz" + idx,
          "args": {
            'compress-output': 'true',
            'collection': 'UnitTestsExport',
            'type': 'json'
          },
          "validate": testParseJSONZipFile
        }, {
          "name": "exportJsonl" + idx,
          "args": {
            'type': 'jsonl',
            'collection': 'UnitTestsExport'
          },
          "validate": testParseJSONlFile
        }, {
          "name": "exportJsonlGz" + idx,
          "args": {
            'compress-output': 'true',
            'collection': 'UnitTestsExport',
            'type': 'jsonl'
          },
          "validate": testParseJSONlZipFile
        }, {
          "name": "exportXgmml" + idx,
          "args": {
            'type': 'xgmml',
            'graph-name': 'UnitTestsExport',
            'collection': 'UnitTestsExport'
          },
          "validate": testParseXGGMLFile
        }, {
          "name":  "exportXgmmlGz" + idx,
          "args": {
            'compress-output': 'true',
            'graph-name': 'UnitTestsExport',
            'collection': 'UnitTestsExport',
            'type': 'xgmml'
          },
          "validate": testParseXGGMLZipFile
        }, {
          "name": "exportQueryFile" + idx,
          "args": {
            'type': 'jsonl',
            'custom-query-file': queryFile,
          },
          "validate": testParseQueryJSONlFile
        }, {
          "name": "exportQuery" + idx,
          "args": {
            'type': 'jsonl',
            'custom-query': 'FOR doc IN UnitTestsExport RETURN doc'
          },
          "validate": testParseQueryJSONlFile
        }, {
          "name": "exportQueryWithBindvars" + idx,
          "args": {
            'type': 'jsonl',
            //these flags have double @s because of the feature that trims the @ for escaping it in configuration files in /etc
            //the double @s will be removed when this feature is deprecated
            'custom-query': 'FOR doc IN @@@@collectionName FILTER doc.value2 == @@value2 RETURN doc',
            'custom-query-bindvars': '{"@@collectionName": "UnitTestsExport", "value2": "this is export"}',
          },
          "validate": testParseQueryJSONlFile
        }, {
          "name": "exportQueryGz" + idx,
          "args": {
            'type': 'jsonl',
            'compress-output': 'true',
            'custom-query': 'FOR doc IN UnitTestsExport RETURN doc'
          },
          "validate": testParseQueryJSONlZipFile 
        }, {
          "name": "exportCsv" + idx,
          "args": {
            'type': 'csv',
            'custom-query': 'FOR doc IN UnitTestsExport RETURN doc',
            'fields': '_key,value1,value2,value3,value4'
          },
          "validate": testReadCSVFile
        }, {
          "name": "exportCsvEscaped" + idx,
          "args": {
            'type': 'csv',
            'custom-query': 'FOR doc IN 1..2 RETURN { value1: 1, value2: [1, 2, 3], value3: true, value4: "foobar" }',
            'fields': 'value1,value2,value3,value4'
          },
          "validate": function() {
            let content = String(fs.read(fs.join(tmpPath, 'query.csv')));
            const expected = `"value1","value2","value3","value4"\n1,"[1,2,3]",true,"foobar"\n1,"[1,2,3]",true,"foobar"\n`;
            if (content !== expected) {
              throw "contents differ!";
            }
          }
        }, {
          "name": "exportCsvEscapedFormulae" + idx,
          "args": {
            'escape-csv-formulae': 'true',
            'type': 'csv',
            'custom-query': 'FOR doc IN 1..2 RETURN { value1: "@foobar", value2: "=HYPERLINK(\\\"evil\\\")", value3: "\\\"some string\\\"", value4: "+line\nbreak" }',
            'fields': 'value1,value2,value3,value4'
          },
          "validate": function() {
            let content = String(fs.read(fs.join(tmpPath, 'query.csv')));
            const expected = `"value1","value2","value3","value4"\n"'@foobar","'=HYPERLINK(""evil"")","""some string""","'+line\nbreak"\n"'@foobar","'=HYPERLINK(""evil"")","""some string""","'+line\nbreak"\n`;
            if (content !== expected) {
              throw "contents differ!";
            }
          }
        }, {
          "name": "exportCsvUnescapedFormulae" + idx,
          "args": {
            'escape-csv-formulae': 'false',
            'type': 'csv',
            'custom-query': 'FOR doc IN 1..2 RETURN { value1: "@foobar", value2: "=HYPERLINK(\\\"evil\\\")", value3: "\\\"some string\\\"", value4: "+line\nbreak" }',
            'fields': 'value1,value2,value3,value4'
          },
          "validate": function() {
            let content = String(fs.read(fs.join(tmpPath, 'query.csv')));
            const expected = `"value1","value2","value3","value4"\n"@foobar","=HYPERLINK(""evil"")","""some string""","+line\nbreak"\n"@foobar","=HYPERLINK(""evil"")","""some string""","+line\nbreak"\n`;
            if (content !== expected) {
              throw "contents differ!";
            }
          }
        }, {
          "name": "exportQueryMaxRuntimeFail" + idx,
          "args": {
            'type': 'jsonl',
            'custom-query': 'RETURN SLEEP(4)',
            'custom-query-max-runtime': '2.0'
          }
        }
      ];
      
      if (!skipEncrypt) {
        exportTestCases.push({
          "name": "exportJsonEncrypt" + idx,
          "args": {
            'encryption.keyfile': keyfile,
            'collection':'UnitTestsExport',
            'type': 'json'
          },
          "validate": function() {
            const decBuffer = fs.readDecrypt(fs.join(tmpPath, 'UnitTestsExport.json'), keyfile);
            JSON.parse(decBuffer);
          }
        });
      }
      exportTestCases.forEach(testCase => {
        print(CYAN + Date() + ': ' + testCase.name + RESET);
        let args = Object.assign(testCase.args, commonArgValues);
        print(GREEN + Date() + " Executing " + testCase.name + RESET);
        let tStart = time();
        try{
          results[testCase.name] = pu.executeAndWait(pu.ARANGOEXPORT_BIN, toArgv(args), this.options, false, tmpPath, this.options.coreCheck, timeout);
          results[testCase.name].duration = time() - tStart;
          if (testCase.hasOwnProperty('validate')) {
            results[testCase.name].failed = results[testCase.name].status ? 0 : 1;
          } else {
            results[testCase.name].status = !results[testCase.name].status;
            results[testCase.name].failed = results[testCase.name].status ? 0 : 1;
          }
        } catch (ex) {
          results.failed += 1;
          results[testCase.name] = {
            failed: 1,
            status: false,
            message: ex,
            duration: time() - tStart
          };
        }
        if (testCase.hasOwnProperty('validate')) {
          print(GREEN + Date() + " Revalidating " + testCase.name + RESET);
          tStart = time();
          try {
            testCase.validate();
            results[testCase.name + "_validate"] = {
              failed: 0,
              status: true
            };
          } catch (e) {
            results.failed += 1;
            results[testCase.name + "_validate"] = {
              failed: 1,
              status: false,
              message: e
            };
          }
          results[testCase.name + "_validate"].duration = time() - tStart;
        }
      });
    });
    if (!skipEncrypt && fs.exists(fs.join(tmpPath, 'ENCRYPTION'))) {
      fs.remove(fs.join(tmpPath, 'ENCRYPTION'));
    }
    return this.shutdown(results);
  }
}

function exportTest (options) {
  return new exportRunner(options, "export").run();
}
exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['export'] = exportTest;

  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
