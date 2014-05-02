/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, */
/*global spyOn, expect, runs, waitsFor*/
/*global templateEngine, $, uiMatchers*/
(function() {
  "use strict";

  describe("Cluster Plan Single Machine View", function() {
    var view, div;

    beforeEach(function() {
      div = document.createElement("div");
      div.id = "content"; 
      document.body.appendChild(div);
      view = new window.PlanTestView();
      window.App = {
        navigate: function() {
          throw "Should be a spy";
        },
        clusterPlan: {
          get: function() {
            throw "Should be a spy";
          }
        }
      };
      spyOn(window.App, "navigate");
    });

    afterEach(function() {
      document.body.removeChild(div);
      delete window.App;
    });

    describe("no stored plan", function() {

      beforeEach(function() {
        spyOn(window.App.clusterPlan, "get").andReturn(undefined);
      });

      it("should render default values", function() {
        spyOn(view.template, "render");
        view.render();
        expect(window.App.clusterPlan.get).toHaveBeenCalledWith("config");
        expect(view.template.render).toHaveBeenCalledWith({
          dbs: 3,
          coords: 2,
          hostname: window.location.hostname,
          port: window.location.port
        });
      });

    });

    describe("with a stored plan", function() {

      var dbs, coords, loc, port;

      beforeEach(function() {
        dbs = 5;
        coords = 3;
        loc = "example.com";
        port = "1337";
        spyOn(window.App.clusterPlan, "get").andReturn({
          numberOfDBservers: dbs,
          numberOfCoordinators: coords,
          dispatchers: {
            d1: {
             endpoint: "http:://" + loc + ":" + port
            }
          }
        });
      });

      it("should render values", function() {
        spyOn(view.template, "render");
        view.render();
        expect(view.template.render).toHaveBeenCalledWith({
          dbs: dbs,
          coords: coords,
          hostname: loc,
          port: port
        });
      });

    });
  });
}());



