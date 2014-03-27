/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, jasmine*/
/*global spyOn, runs, expect, waitsFor*/
/*global templateEngine, GraphView, _, $*/
(function() {
  "use strict";

  describe("Graph View", function() {
    
    var view,
      div;  
    
    beforeEach(function() {
      window.App = {navigate : function(){}};
      div = document.createElement("div");
      div.id = "content"; 
      document.body.appendChild(div);
    });

    afterEach(function() {
      document.body.removeChild(div);
    });

    
    it("should render the correct new lines", function () {
      spyOn(templateEngine, "createTemplate");
      view = new GraphView();
      expect(templateEngine.createTemplate).toHaveBeenCalledWith("graphViewGroupByEntry.ejs");
    });

    describe("after view creation", function() {
      
      var graphs, g1, g2, v1, v2, e1, e2,
      myStore, sys1, cols, fakeStartNode;

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
        myStore = new window.arangoCollections(cols);
        g1 = {
          _id: "_graphs/g1",
          _key: "x1",
          _rev: "123",
          vertices: v1.get("name"),
          edges: e2.get("name")
        };
        g2 = {
          _id: "_graphs/g2",
          _key: "c2",
          _rev: "321",
          vertices: v2.get("name"),
          edges: e1.get("name")
        };
        graphs = new window.GraphCollection();
        graphs.add(g1);
        graphs.add(g2);
        spyOn(graphs, "fetch");
        spyOn(window, "GraphCollection").andCallFake(function() {
          return graphs;
        });
        view = new GraphView({
          collection: myStore,
          graphs: graphs
        });
        fakeStartNode = "vertices/123";
        spyOn($, "ajax").andCallFake(function(opts) {
          if (opts.url === "/_api/simple/any"
            && _.isFunction(opts.success)) {
            opts.success({
              document: {
                _id: fakeStartNode
              }
            });
            return;
          }
          throw "Test not implemented";
        });
        view.render();
      });

      it("should render all vertex collections in correct order", function () {
        var list = document.getElementById("nodeCollection"),
          childs = list.children;
        // v2 should appear before v1
        expect(childs[0].value).toEqual(v2.get("name"));
        expect(childs[1].value).toEqual(v1.get("name"));
        expect(childs.length).toEqual(2);
      });

      it("should render all edge collections in correct order", function () {
        var list = document.getElementById("edgeCollection"),
          childs = list.children;
        // e2 should appear before e1
        expect(childs[0].value).toEqual(e2.get("name"));
        expect(childs[1].value).toEqual(e1.get("name"));
        expect(childs.length).toEqual(2);
      });

      it("should render all edge collections in correct order", function () {
        var list = document.getElementById("graphSelected"),
          childs = list.children;
        // g2 should appear before g1
        expect(childs[0].value).toEqual(g2._key);
        expect(childs[1].value).toEqual(g1._key);
        expect(childs.length).toEqual(2);
      });

      describe("Basic options", function () {

        var defaultGVConfig,
          defaultAdapterConfig;

        beforeEach(function() {
          defaultGVConfig = {
            nodeShaper: {
              label: ["_key"],
              color: {
                type: "attribute",
                key: ["_key"]
              }
            }
          };
          defaultAdapterConfig = {
            type: "arango",
            nodeCollection: undefined,
            edgeCollection: undefined,
            graph: undefined,
            undirected: true,
            baseUrl: "/_db/_system/"
          };
        });

        it("should load a graph with specific vertices and edges", function() {
          $("#nodeCollection").val(v2.get("name"));
          $("#edgeCollection").val(e2.get("name"));
          $("#useCollections").prop("checked", true);
          $("#randomStart").prop("checked", false);
          spyOn(window, "GraphViewerUI");
          $("#createViewer").click();
          // Once load graphs, but never load random node
          expect($.ajax).toHaveBeenCalledWith({
            cache: false,
            type: 'PUT',
            url: '/_api/simple/any',
            contentType: "application/json",
            data: JSON.stringify({collection: v2.get("name")}),
            success: jasmine.any(Function)
          });
          defaultAdapterConfig.nodeCollection = v2.get("name");
          defaultAdapterConfig.edgeCollection = e2.get("name");
          expect(window.GraphViewerUI).toHaveBeenCalledWith(
            $("#content")[0],
            defaultAdapterConfig,
            $("#content").width(),
            680,
            defaultGVConfig,
            fakeStartNode
          );
        });

        it("should load a graph by name", function() {
          $("#graphSelected").val(g2._key);
          $("#useGraphs").prop("checked", true);
          $("#randomStart").prop("checked", false);
          spyOn(window, "GraphViewerUI");
          $("#createViewer").click();
          // Once load graphs, but never load random node
          expect($.ajax).toHaveBeenCalledWith({
            cache: false,
            type: 'PUT',
            url: '/_api/simple/any',
            contentType: "application/json",
            data: JSON.stringify({collection: g2.vertices}),
            success: jasmine.any(Function)
          });
          defaultAdapterConfig.graph = g2._key;
          defaultAdapterConfig.nodeCollection = g2.vertices;
          defaultAdapterConfig.edgeCollection = g2.edges;
          expect(window.GraphViewerUI).toHaveBeenCalledWith(
            $("#content")[0],
            defaultAdapterConfig,
            $("#content").width(),
            680,
            defaultGVConfig,
            fakeStartNode
          );
        });

      });

      describe("Advanced options", function () {

      });

      describe("Graph Management", function () {

        it("should navigate to the management view", function () {
          spyOn(window.App, "navigate");
          $("#manageGraphs").click();
          expect(window.App.navigate).toHaveBeenCalledWith("graphManagement", {trigger: true});
        });
      });

    });


  });

}());
