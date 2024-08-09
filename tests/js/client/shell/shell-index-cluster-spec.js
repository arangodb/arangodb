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
var db = require("org/arangodb").db;
let IM = global.instanceManager;

describe('Cluster collection creation options', function() {
    afterEach(function() {
        db._drop('testi');
    });
    it('should always throw an error when creating a faulty index', function() {
        db._create("testi", {numberOfShards: 1});
        db.testi.save({"test": 1});
        db.testi.save({"test": 1});
        
        expect(function() {
            db.testi.ensureIndex({ type: "hash", fields: [ "test" ], unique: true });
        }).to.throw();
        // before fixing it the second call would return the faulty index
        expect(function() {
            db.testi.ensureIndex({ type: "hash", fields: [ "test" ], unique: true });
        }).to.throw();
    });
    it('should cleanup current after creating a faulty index', function() {
        db._create("testi", {numberOfShards: 1});
        let current = IM.agencyMgr.getFromPlan('Current/Collections/_system');
        let plan = IM.agencyMgr.getFromPlan('Plan/Collections/_system');
        let collectionId = Object.values(plan.arango.Plan.Collections['_system']).reduce((result, collectionDef) => {
            if (result) {
                return result;
            }

            if (collectionDef.name === 'testi') {
                return collectionDef.id;
            }
        }, undefined);
        db.testi.save({"test": 1});
        db.testi.save({"test": 1});
        
        expect(function() {
            db.testi.ensureIndex({ type: "hash", fields: [ "test" ], unique: true });
        }).to.throw();
        // wait for the schmutz
        internal.wait(1.0);
        current = IM.agencyMgr.getFromPlan(`Current/Collections/_system/${collectionId}`);
        Object.values(current.arango.Current.Collections['_system'][collectionId]).forEach(entry => {
            expect(entry.indexes).to.have.lengthOf(1);
        });
    });
});
