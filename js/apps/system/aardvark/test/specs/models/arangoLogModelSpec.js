/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global $*/

(function() {
  "use strict";

  describe("Arango Log Model", function() {

    var model;

    beforeEach(function() {
      model = new window.arangoLog();
    });

    it("verifies urlRoot", function() {
      expect(model.urlRoot).toEqual('/_admin/log');
    });

    it("verifies defaults", function() {
      expect(model.get('lid')).toEqual('');
      expect(model.get('level')).toEqual('');
      expect(model.get('timestamp')).toEqual('');
      expect(model.get('text')).toEqual('');
      expect(model.get('totalAmount')).toEqual('');
    });

  });
}());