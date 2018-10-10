/*jshint globalstrict:true, strict:true, esnext: true */
/*global assertTrue, assertEqual */

"use strict";

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;

function ViewSuite() {
  const colName = 'UnitTestCollection';
  const viewName = 'UnitTestView';

  return {
    ////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////
    setUp: function () {
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////
    tearDown: function () {
      try {
        db._drop(colName);
      } catch(e) {}
      try {
        db._dropView(viewName);
      } catch(e) {}
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief test create & drop of a view with a link.
    /// Regression test for arangodb/backlog#486.
    ////////////////////////////////////////////////////////////////////////////
    testCreateAndDropViewWithLink: function () {
      const col = db._create(colName);
      for (let i = 0; i < 10; i++) {
        col.insert({ i });
      }
      {
        const view = db._createView(viewName, 'arangosearch', {});
        view.properties({ links: { [colName]: { includeAllFields: true } } });
        db._dropView(viewName);
      } // forget variable `view`, it's invalid now
      assertEqual(db[viewName], undefined);
    },
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ViewSuite);

return jsunity.done();
