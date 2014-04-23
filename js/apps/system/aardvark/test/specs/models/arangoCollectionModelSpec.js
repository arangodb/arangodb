/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect, jasmine*/
/*global $, arangoHelper*/

(function() {
  "use strict";

  describe("Arango Collection Model", function() {

    var myCollection;

    beforeEach(function() {
      myCollection = new window.arangoCollectionModel();
    });

    it("verifies urlRoot", function() {
      expect(myCollection.urlRoot).toEqual('/_api/collection');
    });

    it("verifies defaults", function() {
      expect(myCollection.get('id')).toEqual('');
      expect(myCollection.get('name')).toEqual('');
      expect(myCollection.get('status')).toEqual('');
      expect(myCollection.get('type')).toEqual('');
      expect(myCollection.get('isSystem')).toBeFalsy();
      expect(myCollection.get('picture')).toEqual('');
    });

    it("verifies set defaults", function() {
      var name = 'blub',
        status = 'active',
        type   = 'myType';
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

    describe("checking ajax", function() {
      var url, type, shouldFail, successResult, failResult, id, data;

      beforeEach(function() {
        id = "4711";
        successResult = "success";
        failResult = "fail";
        myCollection.set('id', id);
        spyOn($, "ajax").andCallFake(function(opt) {
          expect(opt.url).toEqual(url);
          expect(opt.type).toEqual(type);
          expect(opt.success).toEqual(jasmine.any(Function));
          expect(opt.error).toEqual(jasmine.any(Function));
          if (opt.data) {
            expect(opt.data).toEqual(data);
          }
          if (shouldFail) {
            opt.error(failResult);
          } else {
            opt.success(successResult);
          }
        });
      });

      describe("getProperties", function() {

        beforeEach(function() {
          url = "/_api/collection/" + id + "/properties";
          type = "GET";
        });

        it("success", function() {
          shouldFail = false;
          expect(myCollection.getProperties()).toEqual(successResult);
        });

        it("fail", function() {
          shouldFail = true;
          expect(myCollection.getProperties()).toEqual(failResult);
        });
      });

      describe("getFigures", function() {

        beforeEach(function() {
          url = "/_api/collection/" + id + "/figures";
          type = "GET";
        });

        it("success", function() {
          shouldFail = false;
          expect(myCollection.getFigures()).toEqual(successResult);
        });

        it("fail", function() {
          shouldFail = true;
          expect(myCollection.getFigures()).toEqual(failResult);
        });
      });

      describe("getRevision", function() {

        beforeEach(function() {
          url = "/_api/collection/" + id + "/revision";
          type = "GET";
        });

        it("success", function() {
          shouldFail = false;
          expect(myCollection.getRevision()).toEqual(successResult);
        });

        it("fail", function() {
          shouldFail = true;
          expect(myCollection.getRevision()).toEqual(failResult);
        });
      });

      describe("getFigures", function() {

        beforeEach(function() {
          url = "/_api/index/?collection=" + id;
          type = "GET";
        });

        it("success", function() {
          shouldFail = false;
          expect(myCollection.getIndex()).toEqual(successResult);
        });

        it("fail", function() {
          shouldFail = true;
          expect(myCollection.getIndex()).toEqual(failResult);
        });
      });

      describe("loadCollection", function() {

        beforeEach(function() {
          url = "/_api/collection/" + id + "/load";
          type = "PUT";
        });

        it("success", function() {
          shouldFail = false;
          spyOn(arangoHelper, "arangoNotification");
          myCollection.loadCollection();
          expect(myCollection.get("status")).toEqual("loaded");
        });

        it("fail", function() {
          shouldFail = true;
          spyOn(arangoHelper, "arangoError");
          myCollection.loadCollection();
          expect(arangoHelper.arangoError).toHaveBeenCalledWith("Collection error");
        });
      });

      describe("unloadCollection", function() {

        beforeEach(function() {
          url = "/_api/collection/" + id + "/unload";
          type = "PUT";
        });

        it("success", function() {
          shouldFail = false;
          spyOn(arangoHelper, "arangoNotification");
          myCollection.unloadCollection();
          expect(myCollection.get("status")).toEqual("unloaded");
        });

        it("fail", function() {
          shouldFail = true;
          spyOn(arangoHelper, "arangoError");
          myCollection.unloadCollection();
          expect(arangoHelper.arangoError).toHaveBeenCalledWith("Collection error");
        });
      });

      describe("renameCollection", function() {

        var name;

        beforeEach(function() {
          name = "newname";
          url = "/_api/collection/" + id + "/rename";
          type = "PUT";
          data = JSON.stringify({name: name});
        });

        it("success", function() {
          shouldFail = false;
          expect(myCollection.renameCollection(name)).toBeTruthy();
          expect(myCollection.get("name")).toEqual(name);
        });

        it("fail", function() {
          shouldFail = true;
          expect(myCollection.renameCollection(name)).toBeFalsy();
          expect(myCollection.get("name")).not.toEqual(name);
        });
      });

      describe("changeCollection", function() {
        var wfs, jns;

        beforeEach(function() {
          wfs = true;
          jns = 10 * 100 * 1000;
          url = "/_api/collection/" + id + "/properties";
          type = "PUT";
          data = JSON.stringify({waitForSync: wfs, journalSize: jns});
        });

        it("success", function() {
          shouldFail = false;
          expect(myCollection.changeCollection(wfs, jns)).toBeTruthy();
        });

        it("fail", function() {
          shouldFail = true;
          expect(myCollection.changeCollection(wfs, jns)).toBeFalsy();
        });
      });
    });
  });
}());
