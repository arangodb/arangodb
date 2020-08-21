/*global assertEqual, assertFalse, assertNull, assertTrue, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, bind parameters
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2020-2020 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
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
/// @author Michael Hackstein
/// @author Copyright 2020, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const cn = "UnitTestsAhuacatlUpsert";
const db = require("@arangodb").db;
const jsunity = require("jsunity");

class UpsertTestCase {
  constructor(flags) {
    // TODO parse flags
    this._flags = flags;
    this._expectedResult = new Map();
  }

  /*
   * Public Methods
   */
  name() {
    // TODO generate name
    return `test_${this._flags}`;
  }



  runTest() {
    this._prepareData();

    const query = this._generateQuery();
    // Query will have side effects
    db._query(query, {},  this._options());
    this._validateResult();
    

  }

  /*
   * Private Methods
   */


  _prepareData() {

  }

  _validateResult() {
    const expected = this._generateExpectedResult();
    for (const doc of this._col().toArray()) {
      assertTrue(expected.has(doc.value));
      assertEqua(expected[doc.value], doc.count);
    }

  }

  _generateQuery() {
    return "RETURN 1";
  }

  _options() {
    return {optimizer: {rules: ["+all"]}};
  }

  _generateExpectedResult() {

  }

  _col() {
    return db._collection(cn);
  }


}

function aqlUpsertCombinationSuite () {

  const clear = () => {
    db._drop(cn);
  };

  const testSuite = {
    setUp : () => {
      clear();
      db._create(cn, { numberOfShards: 5 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : clear
  };
  for (let i = 0; i < 1; ++i) {
    const tCase = new UpsertTestCase(i);
    testSuite[tCase.name()] = () => {
      tCase.runTest();
    }
  }  
  return testSuite;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(aqlUpsertCombinationSuite);

return jsunity.done();