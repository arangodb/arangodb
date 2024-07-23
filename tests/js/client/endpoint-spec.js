/* global db, describe, it, beforeEach, afterEach, arango */
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
// / @author Michael Hackstein
// / @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const expect = require('chai').expect;

describe('Endpoint Connection test', function() {
  it('should be able to connect', function() {
    expect(db._version()).to.not.be.undefined;    
  });
  if (require('internal').options()['server.force-json']) {
    it('should have plain json documents', function() {
      let req = arango.GET_RAW('/_api/version');
      expect(req.headers['content-type']).not.to.equal('application/x-velocypack');
    });
  }
});
