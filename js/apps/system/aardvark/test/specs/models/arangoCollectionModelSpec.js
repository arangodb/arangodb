/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global $*/

(function() {
  "use strict";

  var myCollection;

  beforeEach(function() {
    myCollection = new window.arangoCollectionModel();
  });

  describe("Arango Collection Model", function() {
    it("verifies urlRoot", function() {
      expect(myCollection.urlRoot).toEqual('/_api/collection');
    });
  });

  describe("Arango Database Model", function() {
    it("verifies defaults", function() {
      expect(myCollection.get('id')).toEqual('');
      expect(myCollection.get('name')).toEqual('');
      expect(myCollection.get('status')).toEqual('');
      expect(myCollection.get('type')).toEqual('');
      expect(myCollection.get('isSystem')).toBeFalsy();
      expect(myCollection.get('picture')).toEqual('');
    });
  });

  describe("Arango Database Model", function() {
    it("verifies set defaults", function() {
      var name = 'blub',
        status = 'active',
        type   = 'myType',
        myCollection = new window.arangoCollectionModel(
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

  describe("Arango Database Model", function() {
    it("verifies getProperties", function() {
      var id = "4711";
      spyOn($, "ajax").andCallFake(function(opt) {
        expect(opt.url).toEqual("/_api/collection/" + id + "/properties");
        expect(opt.type).toEqual("GET");
      });

      myCollection.getProperties();
    });
  });

  describe("Arango Database Model", function() {
    it("verifies getFigures", function() {
      var id = "4711";
      spyOn($, "ajax").andCallFake(function(opt) {
        expect(opt.url).toEqual("/_api/collection/" + id + "/figures");
        expect(opt.type).toEqual("GET");
      });

      myCollection.getFigures();
    });
  });

  describe("Arango Database Model", function() {
    it("verifies getRevision", function() {
      var id = "4711";
      spyOn($, "ajax").andCallFake(function(opt) {
        expect(opt.url).toEqual("/_api/collection/" + id + "/revision");
        expect(opt.type).toEqual("GET");
      });

      myCollection.getRevision();
    });
  });

  describe("Arango Database Model", function() {
    it("verifies getIndex", function() {
      var id = "4711";
      spyOn($, "ajax").andCallFake(function(opt) {
        expect(opt.url).toEqual("/_api/index/?collection=" + id);
        expect(opt.type).toEqual("GET");
      });

      myCollection.getIndex();
    });
  });

}());