/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, */
/*global spyOn, expect*/
/*global templateEngine, $*/
(function() {
  "use strict";

  describe("Cluster Server View", function() {
    var view, div, dbView;

    beforeEach(function() {
      div = document.createElement("div");
      div.id = "clusterServers"; 
      document.body.appendChild(div);
      dbView = {
        render: function(){}
      };
      spyOn(window, "ClusterDatabaseView").andReturn(dbView);
    });

    afterEach(function() {
      document.body.removeChild(div);
    });

    describe("initialisation", function() {

      it("should create a Cluster Server View", function() {
        view = new window.ClusterServerView();
        expect(window.ClusterDatabaseView).toHaveBeenCalled();
      });

    });

    describe("rendering", function() {

      beforeEach(function() {
        spyOn(dbView, "render");
        view = new window.ClusterServerView();
      });

      it("should not render the Server view", function() {
        view.render();
        expect(dbView.render).not.toHaveBeenCalled();
      });
    });

    describe("user actions", function() {
      var info;

      beforeEach(function() {
        spyOn(dbView, "render");
        view = new window.ClusterServerView();
        view.render();
      });

      it("should be able to navigate to Pavel", function() {
        info = {
          name: "Pavel"
        };
        $("#Pavel").click();
        expect(dbView.render).toHaveBeenCalledWith(info);
      });

      it("should be able to navigate to Peter", function() {
        info = {
          name: "Peter"
        };
        $("#Peter").click();
        expect(dbView.render).toHaveBeenCalledWith(info);
      });

      it("should be able to navigate to Paul", function() {
        info = {
          name: "Paul"
        };
        $("#Paul").click();
        expect(dbView.render).toHaveBeenCalledWith(info);
      });

    });

  });

}());
