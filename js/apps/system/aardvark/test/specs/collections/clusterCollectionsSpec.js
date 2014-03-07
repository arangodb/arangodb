/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, */
/*global spyOn, expect*/
/*global _*/
(function() {

  "use strict";

  describe("Cluster Collections Collection", function() {

    var col, list, c1, c2, c3;

    beforeEach(function() {
      list = [];
      c1 = {name: "Documents", id: "123"};
      c2 = {name: "Edges", id: "321"};
      c3 = {name: "_graphs", id: "222"};
      col = new window.ClusterCollections();
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
        list.push(c1);
        list.push(c2);
        list.push(c3);
        var expected = [];
        expected.push({
          name: c1.name,
          status: "ok"
        });
        expected.push({
          name: c2.name,
          status: "ok"
        });
        expected.push({
          name: c3.name,
          status: "ok"
        });
        expect(col.getList()).toEqual(expected);
      });

    });

  });

}());

