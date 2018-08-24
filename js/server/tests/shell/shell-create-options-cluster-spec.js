/*global describe, it, ArangoAgency, afterEach */

////////////////////////////////////////////////////////////////////////////////
/// @brief cluster collection creation tests
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Andreas Streichardt
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';

const expect = require('chai').expect;

var internal = require("internal");
var db = require("org/arangodb").db;

describe('Cluster collection creation options', function() {
    afterEach(function() {
        db._drop('testi');
    });
    it('should wait for all followers to get in sync when waiting for replication', function() {
        db._create("testi", {replicationFactor: 2, numberOfShards: 32}, {waitForSyncReplication: true});
        let current = ArangoAgency.get('Current/Collections/_system');
        let plan = ArangoAgency.get('Plan/Collections/_system');
        let collectionId = Object.values(plan.arango.Plan.Collections['_system']).reduce((result, collectionDef) => {
            if (result) {
                return result;
            }

            if (collectionDef.name === 'testi') {
                return collectionDef.id;
            }
        }, undefined);

        Object.values(current.arango.Current.Collections['_system'][collectionId]).forEach(entry => {
            expect(entry.servers).to.have.lengthOf(2);
        });
    });

});
