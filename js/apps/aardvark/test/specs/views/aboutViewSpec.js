/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global describe, it, expect, jasmine, spyOn*/
/*global $, d3*/


describe("About View", function() {
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

});
