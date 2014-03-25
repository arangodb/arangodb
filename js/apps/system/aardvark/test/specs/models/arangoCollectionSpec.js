/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global $*/

(function() {
  "use strict";

  describe("Arango Collection Model", function() {
    var myCollection = new window.arangoCollection();
    it("verifies urlRoot", function() {
      expect(myCollection.urlRoot).toEqual('/_api/collection');
    });
  });

  describe("Arango Database Model", function() {
    it("verifies defaults", function() {
      var myCollection = new window.arangoCollection();
      expect(myCollection.get('id')).toEqual('');
      expect(myCollection.get('name')).toEqual('');
      expect(myCollection.get('status')).toEqual('');
      expect(myCollection.get('type')).toEqual('');
      expect(myCollection.isSystem).toBeFalsy();
      expect(myCollection.get('picture')).toEqual('');
    });
  });

  describe("Arango Database Model", function() {
    it("verifies set defaults", function() {
      var name = 'blub',
        status = 'active',
        type   = 'myType',
        myCollection = new window.arangoCollection(
          {
            name    : name,
            status  : status,
            type    : type
          }
        );
      expect(myCollection.get('id')).toEqual('');
      expect(myCollection.get('name')).toEqual(name);
      expect(myCollection.get('status')).toEqual(status);
      expect(myCollection.get('type')).toEqual(type);
      expect(myCollection.isSystem).toBeFalsy();
      expect(myCollection.get('picture')).toEqual('');
    });
  });

}());