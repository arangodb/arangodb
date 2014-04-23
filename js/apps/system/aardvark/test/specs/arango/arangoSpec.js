/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global describe, it, expect, jasmine, spyOn, beforeEach*/
/*global $, d3, arangoHelper*/


describe("Arango Helper", function() {
  "use strict";

  describe("Checking collection types converter", function() {

    var ajaxCB;

    beforeEach(function() {
      ajaxCB = function() {

      };
      spyOn($, "ajax").andCallFake(function(opts) {
        ajaxCB(opts);
      });
    });

    it("if blank collection name", function() {
      var myObject = {},
        dummy;
      myObject.name = "";
      dummy = arangoHelper.collectionType(myObject);
      expect(dummy).toBe("-");
    });

    it("if type document", function() {
      var myObject = {},
        dummy;
      myObject.type = 2;
      myObject.name = "testCollection";
      dummy = arangoHelper.collectionType(myObject);
      expect(dummy).toBe("document");
    });

    it("if type edge", function() {
      var myObject = {},
        dummy;
      myObject.type = 3;
      myObject.name = "testCollection";
      dummy = arangoHelper.collectionType(myObject);
      expect(dummy).toBe("edge");
    });

    it("if type unknown", function() {
      var myObject = {},
        dummy;
      myObject.type = Math.random();
      myObject.name = "testCollection";
      dummy = arangoHelper.collectionType(myObject);
      expect(dummy).toBe("unknown");
    });

  });

  describe("checking html escaping", function() {
    var input = "<&>\"'",
        expected = "&lt;&amp;&gt;&quot;&#39;";
    expect(arangoHelper.escapeHtml(input)).toEqual(expected);
  });

});
