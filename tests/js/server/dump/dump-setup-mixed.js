/*jshint globalstrict:false, strict:false, maxlen:4000, unused:false */

////////////////////////////////////////////////////////////////////////////////
/// @brief setup collections for dump/reload tests
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Wilfried Goesgens
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


const base = require("fs").join(
  require('internal').pathForTesting('server'),
  'dump',
  'dump-setup-common.inc');

const setup = require(base);

(function () {
  setup.cleanup();
  setup.createEmpty();
  setup.createMany();
  setup.createOrder();
  setup.createModifyCollection();
  // will not be supported in cluster: setup.createAutoIncKeyGen();
  setup.createPaddedKeyGen();
  setup.createUUIDKeyGen();
  setup.createStrings();
  setup.createTransactional();
  setup.createPersistent();
  setup.createView();
  setup.createJobs();
  setup.createFoxx();
  setup.createAnalyzers();
})();

return {
  status: true
};
