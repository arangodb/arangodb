/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global $*/

(function() {
  "use strict";

  describe("The current database", function() {
    
    var model, myKey, ajaxVerify, server_result, e, v;

    beforeEach(function() {
      myKey = "graph";
      e = "e";
      v = "v";
      server_result = {
        graph: {
          _id: "_graph/" + myKey,
          _key: myKey,
          _rev: "123541",
          edges: e,
          vertices: v
        },
        code: 201,
        error: false
      };
      ajaxVerify = function() {};
      spyOn($, "ajax").andCallFake(function(opt) {
        ajaxVerify(opt);
        opt.success(server_result);
      });
      model = new window.Graph({
        _key: myKey,
        vertices: v,
        edges: e
      });
    });

    it("should request /_api/graph on save", function() {
      ajaxVerify = function(opt) {
        expect(opt.url).toEqual("/_api/graph");
        expect(opt.type).toEqual("POST");
      };
      model.save();
      expect($.ajax).toHaveBeenCalled();
    });

    it("should store the attributes in the model", function() {
      var id = "_graph/" + myKey,
        rev = "12345";
      server_result.graph._id = id;
      server_result.graph._rev = rev;
      model.save();
      expect(model.get("_id")).toEqual(id);
      expect(model.get("_rev")).toEqual(rev);
      expect(model.get("_key")).toEqual(myKey);
      expect(model.get("edges")).toEqual(e);
      expect(model.get("vertices")).toEqual(v);
      expect(model.get("error")).toBeUndefined();
      expect(model.get("code")).toBeUndefined();
      expect(model.get("graph")).toBeUndefined();
      
    });

    it("should request /_api/graph/_key on delete", function() {
      model.save();
      ajaxVerify = function(opt) {
        expect(opt.url).toEqual("/_api/graph/" + myKey);
        expect(opt.type).toEqual("DELETE");
      };
      model.destroy();
      expect($.ajax).toHaveBeenCalled();
    });



  });

}());
