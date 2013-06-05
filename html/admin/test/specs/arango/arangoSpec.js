/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global describe, it, expect, jasmine, spyOn*/
/*global $, d3*/


describe("Arango Helper", function() {
  "use strict";

  describe("Checking javascript types", function() {

    it("type null", function() {
      var dummy = arangoHelper.RealTypeOf(null);
      expect(dummy).toBe('null');
    });

    it("type array", function() {
      var dummy = arangoHelper.RealTypeOf([1,2,3]);
      expect(dummy).toBe('array');
    });

    it("type date", function() {
      var dummy = arangoHelper.RealTypeOf(new Date);
      expect(dummy).toBe('date');
    });

    it("type regexp", function() {
      var dummy = arangoHelper.RealTypeOf(new RegExp);
      expect(dummy).toBe('regex');
    });

  });

  describe("Checking collection types converter", function() {

    it("if blank collection name", function() {
      var myObject = {};
      myObject.name = "";
      var dummy = arangoHelper.collectionType(myObject);
      expect(dummy).toBe("-");
    });

    it("if type document", function() {
      var myObject = {};
      myObject.type = 2;
      myObject.name = "testCollection";
      var dummy = arangoHelper.collectionType(myObject);
      expect(dummy).toBe("document");
    });

    it("if type edge", function() {
      var myObject = {};
      myObject.type = 3;
      myObject.name = "testCollection";
      var dummy = arangoHelper.collectionType(myObject);
      expect(dummy).toBe("edge");
    });

    it("if type unknown", function() {
      var myObject = {};
      myObject.type = Math.random();
      myObject.name = "testCollection";
      var dummy = arangoHelper.collectionType(myObject);
      expect(dummy).toBe("unknown");
    });

  });

  describe("arango gritter ", function() {
    it("warn. notification", function() {
      var dummy = arangoHelper.arangoNotification("test");
      expect(dummy).toBe(true);
    });
    it("crit. notification", function() {
      var dummy = arangoHelper.arangoError("test");
      expect(dummy).toBe(true);
      $.gritter.removeAll();
    });
  });

});
