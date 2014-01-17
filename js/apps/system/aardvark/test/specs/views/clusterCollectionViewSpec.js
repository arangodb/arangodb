/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, */
/*global spyOn, expect*/
/*global templateEngine, $, uiMatchers*/
(function() {
  "use strict";

  describe("Cluster Collection View", function() {
    var view, div, shardsView;

    beforeEach(function() {
      div = document.createElement("div");
      div.id = "clusterCollections"; 
      document.body.appendChild(div);
      shardsView = {
        render: function() {},
        unrender: function() {}
      };
      spyOn(window, "ClusterShardsView").andReturn(shardsView);
      uiMatchers.define(this);
    });

    afterEach(function() {
      document.body.removeChild(div);
    });

    describe("initialisation", function() {

      it("should create a Cluster Server View", function() {
        view = new window.ClusterCollectionView();
        expect(window.ClusterShardsView).toHaveBeenCalled();
      });

    });

    describe("rendering", function() {
  
      var docs, edges, people, colls,
        checkButtonContent = function(col, cls) {
          var btn = document.getElementById(col.name);
          expect(btn).toBeOfClass("btn");
          expect(btn).toBeOfClass("btn-server");
          expect(btn).toBeOfClass("collection");
          expect(btn).toBeOfClass("btn-" + cls);
          expect($(btn).text()).toEqual(col.name);
        };


      beforeEach(function() {
        docs = {
          name: "Documents",
          status: "ok"
        };
        edges = {
          name: "Edges",
          status: "warning"
        };
        people = {
          name: "People",
          status: "critical"
        };
        colls = [
          docs,
          edges,
          people
        ];
        spyOn(shardsView, "render");
        spyOn(shardsView, "unrender");
        view = new window.ClusterCollectionView();
        view.fakeData.collections = colls;
        view.render();
      });

      it("should not unrender the server view", function() {
        expect(shardsView.render).not.toHaveBeenCalled();
        expect(shardsView.unrender).toHaveBeenCalled();
      });

      it("should render the documents collection", function() {
        checkButtonContent(docs, "success");
      });

      it("should render the edge collection", function() {
        checkButtonContent(edges, "warning");
      });

      it("should render the people collection", function() {
        checkButtonContent(people, "danger");
      });

      it("should offer an unrender function", function() {
        shardsView.unrender.reset();
        view.unrender();
        expect($(div).html()).toEqual("");
        expect(shardsView.unrender).toHaveBeenCalled();
      });


      describe("user actions", function() {
        var info;

        beforeEach(function() {
          view = new window.ClusterCollectionView();
        });

        it("should be able to navigate to Documents", function() {
          info = {
            name: "Documents"
          };
          $("#Documents").click();
          expect(shardsView.render).toHaveBeenCalledWith(info);
        });

        it("should be able to navigate to Edges", function() {
          info = {
            name: "Edges"
          };
          $("#Edges").click();
          expect(shardsView.render).toHaveBeenCalledWith(info);
        });

        it("should be able to navigate to People", function() {
          info = {
            name: "People"
          };
          $("#People").click();
          expect(shardsView.render).toHaveBeenCalledWith(info);
        });

      });
    });


  });

}());


