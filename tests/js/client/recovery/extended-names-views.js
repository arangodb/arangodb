/* jshint globalstrict:false, strict:false, unused: false */
/* global runSetup assertTrue, assertNotEqual, NORMALIZE_STRING */
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
// / @author Jan Steemann
// / @author Copyright 2023, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;
const internal = require('internal');
const jsunity = require('jsunity');
const ArangoView = require("@arangodb").ArangoView;

const extendedName = "Десятую Международную Конференцию по 💩🍺🌧t⛈c🌩_⚡🔥💥🌨";

if (runSetup === true) {
  'use strict';

  db._createView(extendedName, "arangosearch", {});
  return 0;
}

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test that we can still start
    // //////////////////////////////////////////////////////////////////////////////

    testRestart: function () {
      let views = db._views();
      assertNotEqual(-1, views.map((v) => v.name()).indexOf(NORMALIZE_STRING(extendedName)));
      let view = db._view(extendedName);
      assertTrue(view instanceof ArangoView);

      db._dropView(extendedName);
    }

  };
}

jsunity.run(recoverySuite);
return jsunity.done();
