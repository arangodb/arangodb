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
/// @author Alan Plum
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const { context } = require('@arangodb/locals');
const { ArangoError, errors } = require('@arangodb');

context.use(require('./aardvark'));
if (internal.isFoxxApiDisabled()) {
  context.service.router.all('/foxxes/*', (_req, res) => {
    res.throw(403, new ArangoError({
      errorNum: errors.ERROR_SERVICE_API_DISABLED.code,
      errorMessage: errors.ERROR_SERVICE_API_DISABLED.message
    }));
  });
} else {
  context.use('/foxxes', require('./foxxes'));
}
context.use('/cluster', require('./cluster'));
context.use('/statistics', require('./statistics'));
