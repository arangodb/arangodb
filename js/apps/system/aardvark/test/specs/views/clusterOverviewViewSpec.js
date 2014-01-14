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
        // Fake Data Injection to be removed
        view.fakeData = {
          primaries: {
            plan: 3,
            having: 2,
            status: "warning"
          },
          secondaries: {
            plan: 3,
            having: 1,
            status: "critical"
          },
          coordinators: {
            plan: 3,
            having: 3,
            status: "ok"
          }
        };
      });

      it("should not render the Server view", function() {
        view.render();
        expect(serverView.render).not.toHaveBeenCalled();
      });

      it("should render the amounts and status of primaries", function() {
        view.render();
        var btn = document.getElementById("primary"),
          txt = $(btn).text();
        expect(btn).toBeDefined();
        expect(txt.trim()).toEqual("Primaries 2/3");
        expect($(btn).hasClass("btn-success")).toBeFalsy();
        expect($(btn).hasClass("btn-warning")).toBeTruthy();
        expect($(btn).hasClass("btn-danger")).toBeFalsy();
      });

      it("should render the amounts and status of secondaries", function() {
        view.render();
        var btn = document.getElementById("secondary"),
          txt = $(btn).text();
        expect(btn).toBeDefined();
        expect(txt.trim()).toEqual("Secondaries 1/3");
        expect($(btn).hasClass("btn-success")).toBeFalsy();
        expect($(btn).hasClass("btn-warning")).toBeFalsy();
        expect($(btn).hasClass("btn-danger")).toBeTruthy();
      });

      it("should render the amounts and status of coordinators", function() {
        view.render();
        var btn = document.getElementById("coordinator"),
          txt = $(btn).text();
        expect(btn).toBeDefined();
        expect(txt.trim()).toEqual("Coordinators 3/3");
        expect($(btn).hasClass("btn-success")).toBeTruthy();
        expect($(btn).hasClass("btn-warning")).toBeFalsy();
        expect($(btn).hasClass("btn-danger")).toBeFalsy();
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

