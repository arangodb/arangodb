/*global describe, require, beforeEach, it, expect, afterEach, arango */

(function() {
  "use strict";
    
  describe("AQL query analyzer", function() {
    var isServer = false;
    try {
      if (typeof arango.getEndpoint === "function") {
        isServer = false;
      }
    }
    catch (e) {
    }
    var query = "FOR x IN 1..5 LET y = SLEEP(1) RETURN x";
    var internal = require("internal");
    var testee;
    var tasks = require("org/arangodb/tasks");
    var taskInfo = {
      offset: 0,
      command: function () {
        var query = "FOR x IN 1..5 LET y = SLEEP(1) RETURN x";
        require('internal').db._query(query);
      }
    };

    var filterQueries = function(q) {
      return (q.query === query);
    };

    var sendQuery = function(count, async) {
      count = count || 1;
      for (var i = 0; i < count; ++i) {
        if (async === false) {
          internal.db._query(query);
        } else {
          tasks.register(taskInfo);
        }
      }
      if (async === true) {
        internal.wait(1);
      }
    };

    var restoreDefaults = function(testee) {
      testee.properties({ 
        enabled: true,
        trackSlowQueries: true,
        maxSlowQueries: 64,
        slowQueryThreshold: 10,
        maxQueryStringLength: 8192
      });
    };

    beforeEach(function() {
      testee = require("org/arangodb/aql/queries");
      restoreDefaults(testee);
    });

    afterEach(function() {
      restoreDefaults(testee);
    });

    it("should be able to activate the tracking", function() {
      testee.properties({
        enabled: true
      });
      expect(testee.properties().enabled).toBeTruthy();
    });
    
    it("should be able to deactivate the tracking", function() {
      testee.properties({
        enabled: false
      });
      expect(testee.properties().enabled).toBeFalsy();
    });

    it("should be able to set tracking properties", function() {
      testee.properties({
        enabled: true,
        trackSlowQueries: false,
        maxSlowQueries: 42,
        slowQueryThreshold: 2.5,
        maxQueryStringLength: 117
      });
      expect(testee.properties().enabled).toBeTruthy();
      expect(testee.properties().trackSlowQueries).toBeFalsy();
      expect(testee.properties().maxSlowQueries).toEqual(42);
      expect(testee.properties().slowQueryThreshold).toEqual(2.5);
      expect(testee.properties().maxQueryStringLength).toEqual(117);
    });

    describe("with active tracking", function() {

      beforeEach(function() {
        if (internal.debugCanUseFailAt()) {
          internal.debugClearFailAt();
        }
        testee.properties({
          enabled: true,
          slowQueryThreshold: 20
        });
        testee.clearSlow();
      });

      afterEach(function() {
        if (internal.debugCanUseFailAt()) {
          internal.debugClearFailAt();
        }
        var list = testee.current().filter(filterQueries);
        for (var k = 0; k < list.length; ++k) {
          try {
            testee.kill(list[k].id);
          }catch(e) {
          }
        }
      });

      if (isServer && internal.debugCanUseFailAt()) {
        it("should not crash when inserting a query into the current list fails", function() {
          internal.debugSetFailAt("QueryList::insert");

          // inserting the query will fail
          sendQuery(1, true);
          expect(testee.current().filter(filterQueries).length).toEqual(0);
        });
      }

      it("should be able to get currently running queries", function() {
        sendQuery(1, true);
        expect(testee.current().filter(filterQueries).length).toEqual(1);
      });

      it("should not track queries if turned off", function() {
        testee.properties({
          enabled: false
        });
        sendQuery(1, false);
        expect(testee.current().filter(filterQueries).length).toEqual(0);
      });

      it("should work when tracking is turned off in the middle", function() {
        sendQuery(1, true);
        expect(testee.current().filter(filterQueries).length).toEqual(1);

        // turn it off now
        testee.properties({
          enabled: false
        });

        // wait a while, and expect the list of queries to be empty in the end
        var tries = 0;
        while (tries++ < 20) {
          if (testee.current().filter(filterQueries).length === 0) {
            break;
          }
          internal.wait(1);
        }

        expect(testee.current().filter(filterQueries).length).toEqual(0);
      });

      it("should track slow queries by threshold", function() {
        sendQuery(1, false);
        expect(testee.current().filter(filterQueries).length).toEqual(0);
        expect(testee.slow().filter(filterQueries).length).toEqual(0);

        testee.properties({
          slowQueryThreshold: 2
        });

        sendQuery(1, false);
        expect(testee.current().filter(filterQueries).length).toEqual(0);
        expect(testee.slow().filter(filterQueries).length).toEqual(1);
      });

      it("should be able to clear the list of slow queries", function() {
        testee.properties({
          slowQueryThreshold: 2
        });
        sendQuery(1, false);
        expect(testee.slow().filter(filterQueries).length).toEqual(1);
        testee.clearSlow();
        expect(testee.slow().filter(filterQueries).length).toEqual(0);
      });

      it("should track at most n slow queries", function() {
        var max = 2;
        testee.properties({
          slowQueryThreshold: 2,
          maxSlowQueries: max
        });
        sendQuery(3, false);
        expect(testee.current().filter(filterQueries).length).toEqual(0);
        expect(testee.slow().filter(filterQueries).length).toEqual(max);
      });

      if (isServer && internal.debugCanUseFailAt()) {
        it("should not crash when trying to move a query into the slow list", function() {
          internal.debugSetFailAt("QueryList::remove");

          testee.properties({
            slowQueryThreshold: 2
          });
          sendQuery(3, false);
      
          // no slow queries should have been logged  
          expect(testee.slow().filter(filterQueries).length).toEqual(0);
        });
      }

      it("should not track slow queries if turned off", function() {
        testee.properties({
          slowQueryThreshold: 2,
          trackSlowQueries: false
        });
        sendQuery(1, false);
        expect(testee.current().filter(filterQueries).length).toEqual(0);
        expect(testee.slow().filter(filterQueries).length).toEqual(0);
      });

      it("should be able to kill a query", function() {
        sendQuery(1, true);
        var list = testee.current().filter(filterQueries);
        expect(list.length).toEqual(1);
        try {
          testee.kill(list[0].id);
          list = testee.current().filter(filterQueries);
          for (var i = 0; i < 10 && list.length > 0; ++i) {
            internal.wait(0.1);
            list = testee.current().filter(filterQueries);
          }
          expect(list.length).toEqual(0);
        } catch (e) {
          expect(e).toBeUndefined("Could not kill the running query");
        }
      });

    });
  });
}());
