/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, */
/*global spyOn, expect*/
/*global templateEngine, $*/
(function() {
  "use strict";

  describe("Cluster Collection View", function() {
    var view, div, shardsView;

    beforeEach(function() {
      div = document.createElement("div");
      div.id = "clusterCollections"; 
      document.body.appendChild(div);
      shardsView = {
        render: function(){}
      };
      spyOn(window, "ClusterShardsView").andReturn(shardsView);
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

      beforeEach(function() {
        spyOn(shardsView, "render");
        view = new window.ClusterCollectionView();
      });

      it("should not render the Server view", function() {
        view.render();
        expect(shardsView.render).not.toHaveBeenCalled();
      });
    });

    describe("user actions", function() {
      var info;

      beforeEach(function() {
        spyOn(shardsView, "render");
        view = new window.ClusterCollectionView();
        view.render();
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

}());


