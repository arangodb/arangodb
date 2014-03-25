/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global $*/

(function() {
  "use strict";

  describe("foxx Model", function() {
    it("verifies url without _key", function() {
      var myFoxx = new window.Foxx();
      expect(myFoxx.url())
        .toEqual("/_admin/aardvark/foxxes/install");
    });
  });

  describe("foxx Model", function() {
    it("verifies url with _key", function() {
      var key = 'key',
        myFoxx = new window.Foxx(
        {
          _key: key
        }
      );
      expect(myFoxx.url())
        .toEqual("/_admin/aardvark/foxxes/" + key);
    });
  });

  describe("foxx Model", function() {
    it("verifies defaults", function() {
      var myFoxx = new window.Foxx();
      expect(myFoxx.get('title')).toEqual('');
      expect(myFoxx.get('version')).toEqual('');
      expect(myFoxx.get('mount')).toEqual('');
      expect(myFoxx.get('description')).toEqual('');
      expect(myFoxx.get('git')).toEqual('');
      expect(myFoxx.get('isSystem')).toBeFalsy();
      expect(myFoxx.get('development')).toBeFalsy();
    });
  });

  describe("foxx Model", function() {
    it("verifies isNew", function() {
      var myFoxx = new window.Foxx();
      expect(myFoxx.isNew()).toBeFalsy();
    });
  });



}());