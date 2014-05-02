/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, */
/*global spyOn, runs, expect, waitsFor*/
/*global GraphManagementView, _, $*/

(function() {
  "use strict";

  describe("Graph Management View", function() {
    
    var view,
      div,
      graphs,
      collections,
      v1, v2, e1, e2, sys1, cols;
    
    beforeEach(function() {
      collections = new window.arangoCollections(cols);
      graphs = new window.GraphCollection();
      div = document.createElement("div");
      div.id = "content"; 
      document.body.appendChild(div);
      view = new window.GraphManagementView({
        collection: graphs
      }); 
      window.App = window.App || {
        navigate: function(){}
      };
    });

    afterEach(function() {
      document.body.removeChild(div);
    });

    it("should fetch the graphs on render", function () {
      spyOn(graphs, "fetch");
      view.render();
      expect(graphs.fetch).toHaveBeenCalledWith({async: false});
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
        graphs.add(g1);
        graphs.add(g2);
        graphs.add(g3);
        view.render();
      });

      it("should create a sorted list of all graphs", function() {
        var list = $("div.tile h5.collectionName", "#graphManagementThumbnailsIn");
        expect(list.length).toEqual(3);
        // Order would be g2, g3, g1
        expect($(list[0]).html()).toEqual(g2._key);
        expect($(list[1]).html()).toEqual(g3._key);
        expect($(list[2]).html()).toEqual(g1._key);
      });

      describe("creating a new graph", function() {

        it("should navigate to the graph adding view", function() {
          spyOn(window.App, "navigate");
          $("#createGraph").click();
          expect(window.App.navigate).toHaveBeenCalledWith(
            "graphManagement/add",
            {
              trigger: true
            }
          );
        });

      });

      it("should navigate to graph delete view", function() {
        spyOn(window.App, "navigate");
        var lg = graphs.get("g3");
        $("#" + g3._key + "_settings").click();
        $("#deleteGraph").click();
        expect(window.App.navigate).toHaveBeenCalledWith(
          "graphManagement/delete/" + g3._key,
          {
            trigger: true
          }
        );
      });

    });

  });

}());

