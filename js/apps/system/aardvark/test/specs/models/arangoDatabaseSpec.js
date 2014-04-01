/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global $*/

(function() {
  "use strict";

  describe("Arango Database Model", function() {

    var model,
      ajaxVerify;

    beforeEach(function() {
      model = new window.DatabaseModel(
        {
          name: 'myDatabase'
        }
      );
    });

    it("verifies isNew", function() {
      expect(model.isNew()).toBeFalsy();
    });

    it("verifies url", function() {
      expect(model.url).toEqual("/_api/database");
    });

    it("verifies sync _create_", function() {
      var myMethod = 'create',
        myModel = model,
        myOptions = '';

      ajaxVerify = function(opt) {
        expect(opt.url).toEqual('/_api/database');
        expect(opt.type).toEqual('POST');
      };

      spyOn($, "ajax").andCallFake(function(opt) {
        ajaxVerify(opt);
      });

      model.sync(myMethod, myModel, myOptions);
      expect($.ajax).toHaveBeenCalled();

      expect(model.url).toEqual("/_api/database");
    });

    it("verifies sync _update_", function() {
      var myMethod = 'update',
        myModel = model,
        myOptions = '';

      ajaxVerify = function(opt) {
        expect(opt.url).toEqual('/_api/database');
        expect(opt.type).toEqual('POST');
      };

      spyOn($, "ajax").andCallFake(function(opt) {
        ajaxVerify(opt);
      });

      model.sync(myMethod, myModel, myOptions);
      expect($.ajax).toHaveBeenCalled();

      expect(model.url).toEqual("/_api/database");
    });

  });
}());