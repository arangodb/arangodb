/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, */
/*global spyOn, expect*/
/*global _*/
(function() {

  "use strict";

  describe("Cluster Shards Collection", function() {

    var col, list, s1, s2, s3, oldRouter;

    beforeEach(function() {
      oldRouter = window.App;
      window.App = {
        registerForUpdate: function(){},
        addAuth: function(){},
        getNewRoute: function(){}
      };
      list = [];
      s1 = {name: "Pavel", shards: ["asd", "xyz", "123"]};
      s2 = {name: "Perry", shards: []};
      s3 = {name: "Pancho", shards: ["hallo", "hans", "12333"]};
      col = new window.ClusterShards();

      spyOn(col, "fetch").andCallFake(function() {
        _.each(list, function(s) {
          col.add(s);
        });
      });


    });

    describe("list overview", function() {

      it("should fetch the result sync", function() {
        col.fetch.reset();
        col.getList();
        expect(col.fetch).toHaveBeenCalledWith({
          async: false,
          beforeSend: jasmine.any(Function)
        });
      });

      it("should return a list with default ok status", function() {
        list.push(s1);
        list.push(s2);
        list.push(s3);
        var expected = [];
        expected.push({
          server: s1.name,
          shards: s1.shards
        });
        expected.push({
          server: s2.name,
          shards: s2.shards
        });
        expected.push({
          server: s3.name,
          shards: s3.shards
        });
        expect(col.getList()).toEqual(expected);
      });

    });

  });

}());


