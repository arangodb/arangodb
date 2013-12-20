/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/

(function() {
  "use strict";

  describe("The document view", function() {

    var view, model;

    beforeEach(function() {
      model = new window.arangoDocument({
        _key: "123",
        _id: "v/123",
        _rev: "123",
        name: "alice"
      });
      view = new window.DocumentView();
    });

    it("should offer a list view", function() {
    });

    it("should offer a source view", function() {
    });

    it("should offer a way back to the collection", function() {
    });

    it("should offer a way back to the collections overiew", function () {
    });

    it("should offer a way to add more attributes", function() {
    });

    it("should offer the table of document attributes", function() {
    });
  });

}());
