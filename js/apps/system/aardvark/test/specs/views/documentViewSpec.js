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

    it("assert the basics", function () {
      expect(view.colid).toEqual(0);
      expect(view.colid).toEqual(0);
      expect(view.editor).toEqual(0);
      expect(view.events).toEqual({
        "click #saveDocumentButton" : "saveDocument"
      });
    });

    it("check document view type check", function() {
      var model = new window.arangoDocument({
        _key: "123",
        _id: "v/123",
        _rev: "123",
        name: "alice"
      });

      var arangoDocStoreDummy = {
        getEdge: function () {
          return true;
        },
        getDocument: function () {
          return true;
        },
        first: function () {
          return model;
        }
      };

      //spyOn(window, "arangoDocumentStore").andReturn(arangoDocStoreDummy);
      //spyOn(window.arangoDocumentStore, "getEdge").andReturn(true);

      view.typeCheck('edge').toEqual(true);
      view.typeCheck('document').toEqual(true);
      view.typeCheck('xyz').toEqual(false);
      view.typeCheck(false).toEqual(false);
      view.typeCheck(123).toEqual(false);
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
