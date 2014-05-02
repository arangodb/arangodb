/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, */
/*global spyOn, runs, expect, waitsFor, jasmine*/
/*global AddNewGraphView, _, $, arangoHelper */

(function() {
  "use strict";

  describe("Add new Graph View", function() {
    
    var view,
      div,
      graphs,
      collections,
      v1, v2, e1, e2, sys1, cols;
    
    beforeEach(function() {
      var createCol = function(name, type, isSystem) {
        return new window.arangoCollectionModel({
          id: name,
          type: type,
          isSystem: isSystem || false,
          name: name,
          status: "loaded"
        });
      };
      v1 = createCol("z1", "document");
      v2 = createCol("a2", "document");
      e1 = createCol("y1", "edge");
      e2 = createCol("b2", "edge");
      sys1 = createCol("_sys1", "document (system)", true);
      cols = [sys1, v1, v2, e1, e2];
      collections = new window.arangoCollections(cols);
      graphs = new window.GraphCollection();
      div = document.createElement("div");
      div.id = "modalPlaceholder"; 
      document.body.appendChild(div);
      view = new window.AddNewGraphView({
        collection: collections,
        graphs: graphs
      }); 
    });

    afterEach(function() {
      document.body.removeChild(div);
    });

    it("should fetch the collections on render", function () {
      spyOn(graphs, "fetch");
      spyOn(collections, "fetch");
      view.render();
      expect(graphs.fetch).toHaveBeenCalledWith({async: false});
      expect(collections.fetch).toHaveBeenCalledWith({async: false});
    });

    describe("after rendering", function () {

      var g1, g2, g3;

      beforeEach(function() {
        g1 = {
          _id: "_graphs/x1",
          _key: "x1",
          _rev: "123",
          vertices: "v1",
          edges: "e2"
        };
        g2 = {
          _id: "_graphs/c2",
          _key: "c2",
          _rev: "321",
          vertices: "v2",
          edges: "e1"
        };
        g3 = {
          _id: "_graphs/g3",
          _key: "g3",
          _rev: "111",
          vertices: "v3",
          edges: "e3"
        };
        spyOn(graphs, "fetch");
        spyOn(collections, "fetch");
        graphs.add(g1);
        graphs.add(g2);
        graphs.add(g3);
        view.render();
      });
      /*
      it("should offer the list of collections", function() {

      });
      */
      it("should be able to create a new graph", function() {
        var nField = "#newGraphName",
          vField = "#newGraphVertices",
          eField = "#newGraphEdges",
          name = "g",
          v = "v",
          e = "e";
        $(nField).val(name);
        $(vField).val(v);
        $(eField).val(e);
        spyOn(graphs, "create");
        $("#createNewGraph").click();
        expect(graphs.create).toHaveBeenCalledWith({
          _key: name,
          vertices: v,
          edges: e
        }, {
          success: jasmine.any(Function),
          error: jasmine.any(Function)
        });
      });

      describe("if invalid information is added", function() {

        beforeEach(function() {
          spyOn(arangoHelper, "arangoNotification");
          spyOn(graphs, "create");
        });

        it("should alert unnamed graph", function() {
          var nField = "#newGraphName",
            vField = "#newGraphVertices",
            eField = "#newGraphEdges",
            name = "",
            v = "v",
            e = "e";
          $(nField).val(name);
          $(vField).val(v);
          $(eField).val(e);
          $("#createNewGraph").click();
          expect(arangoHelper.arangoNotification)
            .toHaveBeenCalledWith(
              "A name for the graph has to be provided."
            );
          expect(graphs.create).not.toHaveBeenCalled();
        });

        it("should alert unnamed vertices", function() {
          var nField = "#newGraphName",
            vField = "#newGraphVertices",
            eField = "#newGraphEdges",
            name = "g",
            v = "",
            e = "e";
          $(nField).val(name);
          $(vField).val(v);
          $(eField).val(e);
          $("#createNewGraph").click();
          expect(arangoHelper.arangoNotification)
            .toHaveBeenCalledWith(
              "A vertex collection has to be provided."
            );
          expect(graphs.create).not.toHaveBeenCalled();
        });

        it("should alert unnamed edges", function() {
          var nField = "#newGraphName",
            vField = "#newGraphVertices",
            eField = "#newGraphEdges",
            name = "g",
            v = "v",
            e = "";
          $(nField).val(name);
          $(vField).val(v);
          $(eField).val(e);
          $("#createNewGraph").click();
          expect(arangoHelper.arangoNotification)
            .toHaveBeenCalledWith(
              "An edge collection has to be provided."
            );
          expect(graphs.create).not.toHaveBeenCalled();
        });

        it("should alert arango errors", function() {
          var nField = "#newGraphName",
            vField = "#newGraphVertices",
            eField = "#newGraphEdges",
            name = "g",
            v = "v",
            e = "e",
            errMsg = "ArangoDB internal errror";
          $(nField).val(name);
          $(vField).val(v);
          $(eField).val(e);
          graphs.create.andCallFake(function (info, opts) {
            expect(opts.error).toBeDefined();
            opts.error(info, {
              responseText: JSON.stringify({
                error: true,
                code: 400,
                errorNum: 1093,
                errorMessage: errMsg
              })
            });
          });
          spyOn(arangoHelper, "arangoError");
          $("#createNewGraph").click();
          expect(graphs.create).toHaveBeenCalled();
          expect(arangoHelper.arangoError)
            .toHaveBeenCalledWith(errMsg);
        });
      });
    });
  });
}());
