/* global AQL_EXPLAIN, AQL_EXECUTE */
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const internal = require("internal");
const db = arangodb.db;
const {assertEqual} = jsunity.jsUnity.assertions;

function optimizerRuleZkd2dIndexTestSuite() {
    const colName = 'UnitTestZkdIndexCollection';
    let col;

    return {
        setUpAll: function () {
            col = db._create(colName);
        },

        tearDownAll: function () {
            col.drop();
        },

        testNoFieldValueTypes: function () {
            try {
                col.ensureIndex({type: 'zkd', name: 'zkdIndex', fields: ['x', 'y']});
            } catch (e) {
                assertEqual(e.errorNum, internal.errors.ERROR_BAD_PARAMETER.code);
            }
        },

        testSparseProperty: function () {
            try {
                col.ensureIndex({type: 'zkd', name: 'zkdIndex', fields: ['x', 'y'], fieldValueTypes: 'double', sparse: true});
            } catch (e) {
                assertEqual(e.errorNum, internal.errors.ERROR_BAD_PARAMETER.code);
            }
        },

        testArrayExpansions: function () {
            try {
                col.ensureIndex({type: 'zkd', name: 'zkdIndex', fields: ['x[*]', 'y'], fieldValueTypes: 'double'});
            } catch (e) {
                assertEqual(e.errorNum, internal.errors.ERROR_BAD_PARAMETER.code);
            }
        }

    };
}

jsunity.run(optimizerRuleZkd2dIndexTestSuite);

return jsunity.done();
