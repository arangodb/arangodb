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

const fs = require('fs');

const functionsDocumentation = {
  'importing': 'import tests'
};
const optionsDocumentation = [
];

const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const yaml = require('js-yaml');

const testPaths = {
  'importing': [
    tu.pathForTesting('server/import'),
    tu.pathForTesting('common/test-data/import') // our testdata...
  ]
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: importing
// //////////////////////////////////////////////////////////////////////////////

const impTodos = [{
  id: 'skip',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-skip.csv')),
  coll: 'UnitTestsImportCsvSkip',
  type: 'csv',
  create: 'true',
  skipLines: 3
}, {
  id: 'json1',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-1.json')),
  coll: 'UnitTestsImportJson1',
  type: 'json',
  create: undefined
}, {
  id: 'json1gz',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-1.json.gz')),
  coll: 'UnitTestsImportJson1Gz',
  type: 'json',
  create: undefined
}, {
  id: 'json2',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-2.json')),
  coll: 'UnitTestsImportJson2',
  type: 'json',
  create: undefined
}, {
  id: 'json3',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-3.json')),
  coll: 'UnitTestsImportJson3',
  type: 'json',
  create: undefined
}, {
  id: 'json4',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-4.json')),
  coll: 'UnitTestsImportJson4',
  type: 'json',
  create: undefined
}, {
  id: 'json4gz',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-4.json.gz')),
  coll: 'UnitTestsImportJson4Gz',
  type: 'json',
  create: undefined
}, {
  id: 'json5',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-5.json')),
  coll: 'UnitTestsImportJson5',
  type: 'json',
  create: undefined
}, {
  id: 'csvNonoCreate',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-1.csv')),
  coll: 'UnitTestsImportCsvNonoCreate',
  type: 'csv',
  create: 'false'
}, {
  id: 'csv1',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-1.csv')),
  coll: 'UnitTestsImportCsv1',
  type: 'csv',
  create: 'true'
}, {
  id: 'csv1gz',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-1.csv.gz')),
  coll: 'UnitTestsImportCsv1Gz',
  type: 'csv',
  create: 'true'
}, {
  id: 'csv2',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-2.csv')),
  coll: 'UnitTestsImportCsv2',
  type: 'csv',
  create: 'true'
}, {
  id: 'csv3',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-3.csv')),
  coll: 'UnitTestsImportCsv3',
  type: 'csv',
  create: 'true'
}, {
  id: 'csv4',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-4.csv')),
  coll: 'UnitTestsImportCsv4',
  type: 'csv',
  create: 'true',
  separator: ';',
  backslash: true
}, {
  id: 'csv5',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-5.csv')),
  coll: 'UnitTestsImportCsv5',
  type: 'csv',
  create: 'true',
  separator: ';',
  backslash: true
}, {
  id: 'csv6',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-6.csv')),
  coll: 'UnitTestsImportCsv6',
  type: 'csv',
  create: 'true',
  separator: ',',
  ignoreMissing: true
}, {
  id: 'csvconvert',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-noconvert.csv')),
  coll: 'UnitTestsImportCsvConvert',
  type: 'csv',
  create: 'true',
  separator: ',',
  convert: true,
  backslash: true
}, {
  id: 'csvnoconvert',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-noconvert.csv')),
  coll: 'UnitTestsImportCsvNoConvert',
  type: 'csv',
  create: 'true',
  separator: ',',
  convert: false,
  backslash: true
}, {
  id: 'csvtypesboolean',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-types.csv')),
  coll: 'UnitTestsImportCsvTypesBoolean',
  type: 'csv',
  create: 'true',
  separator: ',',
  convert: false,
  datatype: "value=boolean",
}, {
  id: 'csvtypesnumber',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-types.csv')),
  coll: 'UnitTestsImportCsvTypesNumber',
  type: 'csv',
  create: 'true',
  separator: ',',
  convert: false,
  datatype: "value=number",
}, {
  id: 'csvtypesstring',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-types.csv')),
  coll: 'UnitTestsImportCsvTypesString',
  type: 'csv',
  create: 'true',
  separator: ',',
  convert: false,
  datatype: "value=string",
}, {
  id: 'csvtypesprecedence',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-types.csv')),
  coll: 'UnitTestsImportCsvTypesPrecedence',
  type: 'csv',
  create: 'true',
  separator: ',',
  convert: true,
  datatype: "value=string",
}, {
  id: 'csvmergeattributes',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-merge-attrs.csv')),
  coll: 'UnitTestsImportCsvMergeAttributes',
  type: 'csv',
  create: 'true',
  separator: ',',
  convert: true,
  datatype: "value=string",
  mergeAttributes: ["Id=[id]", "IdAndValue=[id]:[value]", "ValueAndId=value:[value]/id:[id]", "_key=[id][value]", "newAttr=[_key]"],
}, {
  id: 'csvmergeattributesInvalid',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-merge-attrs.csv')),
  coll: 'UnitTestsImportCsvMergeAttributesInvalid',
  type: 'csv',
  mergeAttributes: ["Id=[]"],
  expectFailure: true,
}, {
  id: 'csvmergeattributesInvalid2',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-merge-attrs.csv')),
  coll: 'UnitTestsImportCsvMergeAttributesInvalid2',
  type: 'csv',
  mergeAttributes: ["idAndValue=[id[value]"],
  expectFailure: true,
}, {
  id: 'csvmergeattributesInvalid3',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-merge-attrs.csv')),
  coll: 'UnitTestsImportCsvMergeAttributesInvalid3',
  type: 'csv',
  mergeAttributes: ["idAndValue=[idAndValue]"],
  expectFailure: true,
}, {
  id: 'csvnoeol',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-noeol.csv')),
  coll: 'UnitTestsImportCsvNoEol',
  type: 'csv',
  create: 'true',
  separator: ',',
  backslash: true
}, {
  id: 'csvheaders',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-data-without-headers.csv')),
  headers: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-headers.csv')),
  coll: 'UnitTestsImportCsvHeaders',
  type: 'csv',
  create: 'true',
}, {
  id: 'csvbrokenheaders',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-data-without-headers.csv')),
  headers: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-broken-headers.csv')),
  coll: 'UnitTestsImportCsvBrokenHeaders',
  type: 'csv',
  create: 'true',
}, {
  id: 'tsv1',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-1.tsv')),
  coll: 'UnitTestsImportTsv1',
  type: 'tsv',
  create: 'true'
}, {
  id: 'tsv1gz',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-1.tsv.gz')),
  coll: 'UnitTestsImportTsv1Gz',
  type: 'tsv',
  create: 'true'
}, {
  id: 'tsv2',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-2.tsv')),
  coll: 'UnitTestsImportTsv2',
  type: 'tsv',
  create: 'true'
}, {
  id: 'edge',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-edges.json')),
  coll: 'UnitTestsImportEdge',
  type: 'json',
  create: 'false'
}, {
  id: 'edgegz',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-edges.json.gz')),
  coll: 'UnitTestsImportEdgeGz',
  type: 'json',
  create: 'false'
}, {
  id: 'unique',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-ignore.json')),
  coll: 'UnitTestsImportIgnore',
  type: 'json',
  create: 'false',
  onDuplicate: 'ignore'
}, {
  id: 'unique',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-unique-constraints.json')),
  coll: 'UnitTestsImportUniqueConstraints',
  type: 'json',
  create: 'false',
  onDuplicate: 'replace'
}, {
  id: 'removeAttribute',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-1.csv')),
  coll: 'UnitTestsImportRemoveAttribute',
  type: 'csv',
  create: 'true',
  removeAttribute: 'a'
}, {
  id: 'createDB',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-1.json')),
  coll: 'UnitTestsImportJson1',
  type: 'json',
  create: 'true',
  database: 'UnitTestImportCreateDatabase',
  createDatabase: 'true'
}, {
  id: 'importUnicode1',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-1.json')),
  coll: 'UnitTestsImportJson1',
  type: 'json',
  create: 'true',
  database: 'maçã',
}, {
  id: 'importUnicode2',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-1.json')),
  coll: 'UnitTestsImportJson1',
  type: 'json',
  create: 'true',
  database: '😀',
}, {
  id: 'createUnicode1',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-1.json')),
  coll: 'UnitTestsImportJson1',
  type: 'json',
  create: 'true',
  database: 'ﻚﻠﺑ ﻞﻄﻴﻓ',
  createDatabase: 'true'
}, {
  id: 'createUnicode2',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-1.json')),
  coll: 'UnitTestsImportJson1',
  type: 'json',
  create: 'true',
  database: 'abc mötor !" \' & <>',
  createDatabase: 'true'
}, {
  id: 'importDataBatchSizeWithoutHeaderFile',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-data-with-header.csv')),
  coll: 'UnitTestsImportDataBatchSizeWithoutHeaderFile',
  type: 'csv',
  create: 'true',
  batchSize: 10,
}, {
  id: 'importDataBatchSizeWithoutHeaderFile2',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-data-with-header.csv')),
  coll: 'UnitTestsImportDataBatchSizeWithoutHeaderFile2',
  type: 'csv',
  create: 'true',
  batchSize: 1000
}, {
  id: 'importDataBatchSizeWithHeaderFile',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-data-without-headers.csv')),
  headers: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-headers.csv')),
  coll: 'UnitTestsImportDataBatchSizeWithHeaderFile',
  type: 'csv',
  create: 'true',
  batchSize: 10,
}, {
  id: 'importDataBatchSizeWithHeaderFile2',
  data: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-data-without-headers.csv')),
  headers: tu.makePathUnix(fs.join(testPaths.importing[1], 'import-headers.csv')),
  coll: 'UnitTestsImportDataBatchSizeWithHeaderFile2',
  type: 'csv',
  create: 'true',
  batchSize: 1000
}];

function importing (options) {
  let instanceInfo = pu.startInstance('tcp', options, {}, 'importing');

  if (instanceInfo === false) {
    return {
      'failed': 1,
      'importing': {
        failed: 1,
        status: false,
        message: 'failed to start server!'
      }
    };
  }

  let result = { failed: 0 };

  try {
    result.setup = tu.runInArangosh(
      options,
      instanceInfo,
      tu.makePathUnix(fs.join(testPaths.importing[0],'import-setup.js')));

    result.setup.failed = 0;
    if (result.setup.status !== true) {
      result.setup.failed = 1;
      result.failed += 1;
      throw new Error('cannot start import setup');
    }

    for (let i = 0; i < impTodos.length; i++) {
      const impTodo = impTodos[i];

      result[impTodo.id] = pu.run.arangoImport(options, instanceInfo, impTodo, options.coreCheck);
      result[impTodo.id].failed = 0;

      if (impTodo.expectFailure) {
        // if status === false, we make true out of it
        // if status === true, we make false out of it
        result[impTodo.id].status = !result[impTodo.id].status;
      }

      if (result[impTodo.id].status !== true && !options.force) {
        result[impTodo.id].failed = 1;
        result.failed += 1;
        throw new Error('cannot run import');
      }
    }

    result.check = tu.runInArangosh(
      options,
      instanceInfo,
      tu.makePathUnix(fs.join(testPaths.importing[0], 'import.js')));

    result.check.failed = result.check.success ? 0 : 1;

    result.teardown = tu.runInArangosh(
      options,
      instanceInfo,
      tu.makePathUnix(fs.join(testPaths.importing[0], 'import-teardown.js')));

    result.teardown.failed = result.teardown.success ? 0 : 1;
  } catch (banana) {
    print('An exceptions of the following form was caught:',
          yaml.safeDump(banana));
  }

  print('Shutting down...');
  result['shutdown'] = pu.shutdownInstance(instanceInfo, options);
  print('done.');

  return result;
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['importing'] = importing;
  defaultFns.push('importing');
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
