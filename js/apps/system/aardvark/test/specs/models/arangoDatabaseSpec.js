/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global $*/

(function() {
  "use strict";

  describe("Arango Database Model", function() {
    it("verifies isNew", function() {
      var myDatabase = new window.Database();
      expect(myDatabase.isNew()).toBeFalsy();
    });
  });

  describe("Arango Database Model", function() {
    it("verifies url", function() {
      var myDatabase = new window.Database();
      expect(myDatabase.url).toEqual("/_api/database");
    });
  });

}());