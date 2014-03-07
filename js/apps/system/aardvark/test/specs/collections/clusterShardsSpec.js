/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, */
/*global spyOn, expect*/
/*global _*/
(function() {

  "use strict";

  describe("Cluster Shards Collection", function() {

    var col, list, s1, s2, s3;

    beforeEach(function() {
      list = [];
      s1 = {id: "Shard 1"};
      s2 = {id: "Shard 2"};
      s3 = {id: "Shard 3"};
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
          async: false
        });
      });
      

      it("should return a list with default ok status", function() {
        list.push(s1);
        list.push(s2);
        list.push(s3);
        var expected = [];
        expected.push({
          id: s1.id,
          status: "ok"
        });
        expected.push({
          id: s2.id,
          status: "ok"
        });
        expected.push({
          id: s3.id,
          status: "ok"
        });
        expect(col.getList()).toEqual(expected);
      });

    });

  });

}());


