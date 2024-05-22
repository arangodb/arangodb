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
/// @author Tobias Gödderz
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const internal = require("internal");
const db = arangodb.db;
const {assertEqual} = jsunity.jsUnity.assertions;

function optimizerRuleMdi2dIndexTestSuite() {
    const colName = 'UnitTestMdiIndexCollection';
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
                col.ensureIndex({type: 'mdi', name: 'mdiIndex', fields: ['x', 'y']});
            } catch (e) {
                assertEqual(e.errorNum, internal.errors.ERROR_BAD_PARAMETER.code);
            }
        },

        testSparseProperty: function () {
            try {
                col.ensureIndex({type: 'mdi', name: 'mdiIndex', fields: ['x', 'y'], fieldValueTypes: 'double', sparse: true});
            } catch (e) {
                assertEqual(e.errorNum, internal.errors.ERROR_BAD_PARAMETER.code);
            }
        },

        testArrayExpansions: function () {
            try {
                col.ensureIndex({type: 'mdi', name: 'mdiIndex', fields: ['x[*]', 'y'], fieldValueTypes: 'double'});
            } catch (e) {
                assertEqual(e.errorNum, internal.errors.ERROR_BAD_PARAMETER.code);
            }
        }

    };
}

jsunity.run(optimizerRuleMdi2dIndexTestSuite);

return jsunity.done();
