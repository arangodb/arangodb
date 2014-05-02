/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global $*/

(function() {
  "use strict";

  describe("Arango Replication Model", function() {
    var myModel = new window.Replication();

    it("verifies defaults", function() {
      expect(myModel.get('state')).toEqual({});
      expect(myModel.get('server')).toEqual({});
    });

  });




}());