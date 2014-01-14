/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, */
/*global spyOn, expect*/
/*global templateEngine, $*/
(function() {
  "use strict";

  describe("Cluster Overview View", function() {
    var view, div, serverView;

    beforeEach(function() {
      div = document.createElement("div");
      div.id = "clusterOverview"; 
      document.body.appendChild(div);
      serverView = {
        render: function(){}
      };
      spyOn(window, "ClusterServerView").andReturn(serverView);
    });

    afterEach(function() {
      document.body.removeChild(div);
    });

    describe("initialisation", function() {

      it("should create a Cluster Server View", function() {
        view = new window.ClusterOverviewView();
        expect(window.ClusterServerView).toHaveBeenCalled();
      });

    });

    describe("rendering", function() {

      beforeEach(function() {
        spyOn(serverView, "render");
        view = new window.ClusterOverviewView();
      });

      it("should not render the Server view", function() {
        view.render();
        expect(serverView.render).not.toHaveBeenCalled();
      });
    });

    describe("user actions", function() {
      var info;

      beforeEach(function() {
        spyOn(serverView, "render");
        view = new window.ClusterOverviewView();
        view.render();
      });

      it("should be able to navigate to primary servers", function() {
        info = {
          type: "primary"
        };
        $("#primary").click();
        expect(serverView.render).toHaveBeenCalledWith(info);
      });

      it("should be able to navigate to primary servers", function() {
        info = {
          type: "secondary"
        };
        $("#secondary").click();
        expect(serverView.render).toHaveBeenCalledWith(info);
      });

      it("should be able to navigate to primary servers", function() {
        info = {
          type: "coordinator"
        };
        $("#coordinator").click();
        expect(serverView.render).toHaveBeenCalledWith(info);
      });

    });

  });

}());

