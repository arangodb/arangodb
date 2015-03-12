/*global describe, require, beforeEach, it, expect, afterEach*/

(function() {
  "use strict";

  describe("AQL query analyzer", function() {

    var testee;
    var tasks = require("org/arangodb/tasks");
    var taskInfo = {
      offset: 0,
      command: function () {
        require('internal').db._query("FOR x IN 1..5 LET y = SLEEP(1) RETURN x");
      }
    };

    var sendQuery = function(count, async) {
      count = count || 1;
      for (var i = 0; i < count; ++i) {
        if (async === false) {
          require('internal').db._query("FOR x IN 1..5 LET y = SLEEP(1) RETURN x");
        } else {
          tasks.register(taskInfo);
        }
      }
      if (async === true) {
        require("internal").wait(1);
      }
    };


    beforeEach(function() {
      testee = require("org/arangodb/aql/queries");
    });

    it("should be able to activate the tracking", function() {
      testee.properties({
        enabled: true
      });
      expect(testee.properties().enabled).toBeTruthy();
    });

    describe("with active tracking", function() {

      beforeEach(function() {
        testee.properties({
          enabled: true,
          slowQueryThreshold: 20
        });
        testee.clearSlow();
      });

      afterEach(function() {
        var list = testee.current();
        for (var k = 0; k < list.length; ++k) {
          try {
            testee.kill(list[k].id);
          }catch(e) {
          }
        }
      });

      it("should be able to get currently running queries", function() {
        sendQuery(1, true);
        expect(testee.current().length).toEqual(1);
      });

      it("should track slow queries by threshold", function() {
        sendQuery(1, false);
        expect(testee.current().length).toEqual(0);
        expect(testee.slow().length).toEqual(0);

        testee.properties({
          slowQueryThreshold: 2
        });

        sendQuery(1, false);
        expect(testee.current().length).toEqual(0);
        expect(testee.slow().length).toEqual(1);
      });

      it("should be able to clear the list of slow queries", function() {
        testee.properties({
          slowQueryThreshold: 2
        });
        sendQuery(1, false);
        expect(testee.slow().length).toEqual(1);
        testee.clearSlow();
        expect(testee.slow().length).toEqual(0);
      });

      it("should track at most n slow queries", function() {
        var max = 2;
        testee.properties({
          slowQueryThreshold: 2,
          maxSlowQueries: max
        });
        sendQuery(3, false);
        expect(testee.current().length).toEqual(0);
        expect(testee.slow().length).toEqual(max);
      });

      it("should not track slow queries if turned off", function() {
        testee.properties({
          slowQueryThreshold: 2,
          trackSlowQueries: false
        });
        sendQuery(1, false);
        expect(testee.current().length).toEqual(0);
        expect(testee.slow().length).toEqual(0);
      });

      it("should be able to kill a query", function() {
        sendQuery(1, true);
        var list = testee.current();
        expect(list.length).toEqual(1);
        try {
          testee.kill(list[0].id);
          list = testee.current();
          for (var i = 0; i < 10 && list.length > 0; ++i) {
            require("internal").wait(0.1);
            list = testee.current();
          }
          expect(list.length).toEqual(0);
        } catch (e) {
          expect(e).toBeUndefined("Could not kill the running query");
        }
      });

    });
  });
}());
