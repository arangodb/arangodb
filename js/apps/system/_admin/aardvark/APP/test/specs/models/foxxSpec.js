/*jshint browser: true */
/*jshint unused: false */
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global $, jasmine*/

(function() {
  "use strict";

  describe("foxx Model", function() {

    it("is sorted by mount", function() {
      expect(window.Foxx.prototype.idAttribute).toEqual("mount");
    });

    it("can return it's mount url-encoded", function() {
      var testMount = "/this/is_/a/test/mount";
      var expected = encodeURIComponent(testMount);
      var myFoxx = new window.Foxx({
        mount: testMount
      });
      expect(myFoxx.encodedMount()).toEqual(expected);
    });

    it("verifies defaults", function() {
      var myFoxx = new window.Foxx();
      expect(myFoxx.get('name')).toEqual('');
      expect(myFoxx.get('version')).toEqual('Unknown Version');
      expect(myFoxx.get('description')).toEqual('No description');
      expect(myFoxx.get('git')).toEqual('');
      expect(myFoxx.get('isSystem')).toBeFalsy();
      expect(myFoxx.get('development')).toBeFalsy();
    });

    it("is always considered newNew", function() {
      var myFoxx = new window.Foxx();
      expect(myFoxx.isNew()).toBeFalsy();
    });
    
    describe("configuration", function() {
      var myFoxx;

      beforeEach(function() {
        var testMount = "/this/is_/a/test/mount";
        myFoxx = new window.Foxx({
          mount: testMount
        });

      });

      it("can be read", function() {
        var data = {
          opt1: {
            type: "string",
            description: "Option description",
            default: "empty",
            current: "empty"
          }
        };
        spyOn($, "ajax").andCallFake(function(opts) {
          expect(opts.url).toEqual("/_admin/aardvark/foxxes/config?mount=" + myFoxx.encodedMount());
          expect(opts.type).toEqual("GET");
          expect(opts.success).toEqual(jasmine.any(Function));
          expect(opts.error).toEqual(jasmine.any(Function));
          opts.success(data);
        });
        myFoxx.getConfiguration(function(result) {
          expect(result).toEqual(data);
        });
        expect($.ajax).toHaveBeenCalled();
      });

      it("can be saved", function() {
        var data = {
          opt1: "empty"
        };
        var call = {
          back: function() {
            throw new Error("Should be a spy.");
          }
        };
        spyOn($, "ajax").andCallFake(function(opts) {
          expect(opts.url).toEqual("/_admin/aardvark/foxxes/config?mount=" + myFoxx.encodedMount());
          expect(opts.type).toEqual("PATCH");
          expect(opts.data).toEqual(JSON.stringify(data));
          expect(opts.success).toEqual(jasmine.any(Function));
          expect(opts.error).toEqual(jasmine.any(Function));
          opts.success(data);
        });
        spyOn(call, "back");
        myFoxx.setConfiguration(data, call.back); 
        expect($.ajax).toHaveBeenCalled();
        expect(call.back).toHaveBeenCalled();
      });

    });
  });

}());
