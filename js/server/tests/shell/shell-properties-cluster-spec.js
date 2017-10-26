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
const internal = require("internal");
const errors = require('@arangodb').errors;
const db = require("@arangodb").db;

const cn1 = "UnitTestPropertiesLeader";
const cn2 = "UnitTestPropertiesFollower";

function checkReplicationFactor(name, fac) {
    let current = ArangoAgency.get('Current/Collections/_system');
    let plan = ArangoAgency.get('Plan/Collections/_system');
    let collectionId = Object.values(plan.arango.Plan.Collections['_system']).reduce((result, collectionDef) => {
        if (result) {
            return result;
        }
        if (collectionDef.name === name) {
            return collectionDef.id;
        }
    }, undefined);

    Object.values(current.arango.Current.Collections['_system'][collectionId]).forEach(entry => {
        expect(entry.servers).to.have.lengthOf(fac);
    });
};

describe('Update collection properties', function() {

    beforeEach(function() {
        db._useDatabase("_system");
    });

    afterEach(function() {
        db._useDatabase("_system");
        try {
            db._drop(cn1);
        } catch (e) {}
    });

    it('increase replication factor ', function() {
        db._create(cn1, {replicationFactor: 1, numberOfShards: 2}, {waitForSyncReplication: true});

        checkReplicationFactor(cn1, 1)

        const coll = db._collection(cn1);
        
        let props = coll.properties({replicationFactor: 2});
        expect(props.replicationFactor).to.equal(2);

        // FIXME: do not wait for a fixed time
        internal.sleep(8);

        checkReplicationFactor(cn1, 2);
    });
    
    it('decrease replication factor ', function() {
        db._create(cn1, {replicationFactor: 2, numberOfShards: 2}, {waitForSyncReplication: true});

        checkReplicationFactor(cn1, 2)

        const coll = db._collection(cn1);

        let props = coll.properties({replicationFactor: 1});
        expect(props.replicationFactor).to.equal(1);

        internal.sleep(8);

        checkReplicationFactor(cn1, 1);
    });

    it('invalid replication factor', function() {
        db._create(cn1, {replicationFactor: 2, numberOfShards: 2}, {waitForSyncReplication: true});

        checkReplicationFactor(cn1, 2)

        try {
            const coll = db._collection(cn1);
            coll.properties({replicationFactor: -1});
            expect(false.replicationFactor).to.equal(true, 
                "Was able to update replicationFactor of follower");
        } catch(e) {
            expect(e.errorNum).to.equal(errors.ERROR_BAD_PARAMETER.code);
        }

        try {
            const coll = db._collection(cn1);
            coll.properties({replicationFactor: 100});
            expect(false.replicationFactor).to.equal(true, 
                "Was able to update replicationFactor of follower");
        } catch(e) {
            expect(e.errorNum).to.equal(errors.ERROR_BAD_PARAMETER.code);
        }

        try {
            const coll = db._collection(cn1);
            coll.properties({replicationFactor: "satellite"});
            expect(false.replicationFactor).to.equal(true, 
                "Was able to update replicationFactor of follower");
        } catch(e) {
            expect(e.errorNum).to.equal(errors.ERROR_BAD_PARAMETER.code);
        }
    });
});


describe('Update collection properties with distributeShardsLike, ', function() {


    beforeEach(function() {
        db._useDatabase("_system");
    });

    afterEach(function() {
        db._useDatabase("_system");

        try {
            db._drop(cn2);            
        } catch (e) {}
        
        try {
            db._drop(cn1);
        } catch (e) {}
    });

    it('increase replication factor', function() {
        db._create(cn1, {replicationFactor: 1, numberOfShards: 2}, {waitForSyncReplication: true});
        db._create(cn2, {distributeShardsLike: cn1}, {waitForSyncReplication: true});

        checkReplicationFactor(cn1, 1);
        checkReplicationFactor(cn2, 1);   

        const leader = db._collection(cn1);
        let props = leader.properties({replicationFactor: 2});
        expect(props.replicationFactor).to.equal(2);

        // FIXME: do not wait for a fixed time
        internal.sleep(8);

        checkReplicationFactor(cn1, 2);
        checkReplicationFactor(cn2, 2); 
    });
    
    it('decrease replication factor', function() {
        db._create(cn1, {replicationFactor: 2, numberOfShards: 2}, {waitForSyncReplication: true});
        db._create(cn2, {distributeShardsLike: cn1}, {waitForSyncReplication: true});

        checkReplicationFactor(cn1, 2);
        checkReplicationFactor(cn2, 2); 

        const leader = db._collection(cn1);

        let props = leader.properties({replicationFactor: 1});
        expect(props.replicationFactor).to.equal(1);

        // FIXME: do not wait for a fixed time
        internal.sleep(8);

        checkReplicationFactor(cn1, 1);
        checkReplicationFactor(cn2, 1);
    });

    it('change replicationFactor of follower', function() {
        db._create(cn1, {replicationFactor: 2, numberOfShards: 2}, {waitForSyncReplication: true});
        db._create(cn2, {distributeShardsLike: cn1}, {waitForSyncReplication: true});

        checkReplicationFactor(cn1, 2);
        checkReplicationFactor(cn2, 2); 
        
        try {
            const follower = db._collection(cn2);
            follower.properties({replicationFactor: 1});
            expect(false.replicationFactor).to.equal(true, 
                "Was able to update replicationFactor of follower");
        } catch(e) {
            expect(e.errorNum).to.equal(errors.ERROR_FORBIDDEN.code);
        }
    });
});