/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global $*/

(function() {
  "use strict";

  describe("arangoDatabase", function() {
    
    var col;

    beforeEach(function() {
        col = new window.ArangoDatabase();
    });

    it("comparator", function() {
        var a = {
            get: function() {
                return "ABCDE";
            }
        }
        expect(col.comparator(a)).toEqual("abcde");
    });

      it("sync", function() {
          spyOn(Backbone, "sync");
          var result = col.sync("read", {url: function () {return "abc"; }}, {url: "default"});
          expect(Backbone.sync).toHaveBeenCalled();
          var objectArg = Backbone.sync.mostRecentCall.args[2];
          expect(objectArg.url).toEqual('abcuser');


      });

      it("url", function() {
          expect(col.url()).toEqual("/_api/database/");


      });

      it("parse undefined reponse", function() {
          expect(col.parse()).toEqual(undefined);


      });

      it("parse defined reponse", function() {
          expect(col.parse({result : {name : "horst"}})).toEqual([{name : "horst"}]);
      });


      it("getDatabases", function() {
          expect(col.getDatabases()).toEqual([]);
      });

      it("createDatabaseURL", function() {
          expect(col.createDatabaseURL("blub", "SSL", 21)).toEqual(
              "https://localhost:21/_db/blub/_admin/aardvark/standalone.html"
          );
      });

      it("createDatabaseURL with http and application", function() {
          window.location.hash = "#application/abc";
          expect(col.createDatabaseURL("blub", "http", 21)).toEqual(
              "http://localhost:21/_db/blub/_admin/aardvark/standalone.html#applications"
          );
      });

      it("createDatabaseURL with http and collection", function() {
          window.location.hash = "#collection/abc";
          expect(col.createDatabaseURL("blub", undefined, 21)).toEqual(
              "http://localhost:21/_db/blub/_admin/aardvark/standalone.html#collections"
          );
      });

      it("should getCurrentDatabase and succeed", function() {
          spyOn($, "ajax").andCallFake(function(opt) {
              expect(opt.url).toEqual("/_api/database/current");
              expect(opt.type).toEqual("GET");
              expect(opt.contentType).toEqual("application/json");
              expect(opt.processData).toEqual(false);
              expect(opt.cache).toEqual(false);
              expect(opt.async).toEqual(false);
              opt.success({code : 200, result : {name : "eee"}});
          });
          var result = col.getCurrentDatabase();
          expect(result).toEqual("eee");
      });

      it("should getCurrentDatabase and succeed with 201", function() {
          spyOn($, "ajax").andCallFake(function(opt) {
              expect(opt.url).toEqual("/_api/database/current");
              expect(opt.type).toEqual("GET");
              expect(opt.contentType).toEqual("application/json");
              expect(opt.processData).toEqual(false);
              expect(opt.cache).toEqual(false);
              expect(opt.async).toEqual(false);
              opt.success({code : 201, result : {name : "eee"}});
          });
          var result = col.getCurrentDatabase();
          expect(result).toEqual({code : 201, result : {name : "eee"}});
      });

      it("should getCurrentDatabase and fail", function() {
          spyOn($, "ajax").andCallFake(function(opt) {
              expect(opt.url).toEqual("/_api/database/current");
              expect(opt.type).toEqual("GET");
              expect(opt.contentType).toEqual("application/json");
              expect(opt.processData).toEqual(false);
              expect(opt.cache).toEqual(false);
              expect(opt.async).toEqual(false);
              opt.error({code : 401, result : {name : "eee"}});
          });
          var result = col.getCurrentDatabase();
          expect(result).toEqual({code : 401, result : {name : "eee"}});

      });

  });

}());

