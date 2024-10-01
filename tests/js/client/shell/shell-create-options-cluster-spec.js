/*global describe, it, ArangoAgency, afterEach, arango */

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
/// @author Andreas Streichardt
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const expect = require('chai').expect;

var internal = require("internal");
const { instanceManager } = require('../../../../js/client/modules/@arangodb/testutils/instance-manager');
var db = require("org/arangodb").db;

describe('Cluster collection creation options', function() {
    afterEach(function() {
        db._drop('testi');
    });
    it('should wait for all followers to get in sync when waiting for replication', function() {
        db._create("testi", {replicationFactor: 2, numberOfShards: 32}, {waitForSyncReplication: true});
        let current = global.instanceManager.agencyMgr.getFromPlan('Current/Collections/_system');
        let plan = global.instanceManager.agencyMgr.getFromPlan('Plan/Collections/_system');
        
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
