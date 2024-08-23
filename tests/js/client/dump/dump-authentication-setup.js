/*jshint globalstrict:false, strict:false, maxlen:4000, unused:false */

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
/// @author Jan Steemann
/// @author Wilfried Goesgens
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////


const base = require("fs").join(
  require('internal').pathForTesting('client'),
  'dump',
  'dump-setup-common.inc');
const setup = require(base);

(function () {
  setup.cleanup();
  setup.createEmpty();
  setup.createExtendedName();
  setup.createAutoIncKeyGen();
  setup.createUsers();
  setup.createMany();
  setup.createOrder();
  setup.createSmartGraph3_11_compat();
  setup.createFoxx();
})();

return {
  status: true
};
