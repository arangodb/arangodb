/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, */
/*global spyOn, expect*/
/*global templateEngine, $, uiMatchers*/
(function() {
  "use strict";

  describe("Cluster Coordinator View", function() {
    var view, div;

    beforeEach(function() {
      div = document.createElement("div");
      div.id = "clusterServers"; 
      document.body.appendChild(div);
      uiMatchers.define(this);
    });

    afterEach(function() {
      document.body.removeChild(div);
    });

    describe("rendering", function() {
      
      var charly, carlos, chantalle, coordinators,
        getTile = function(name) {
          return document.getElementById(name);
        },
        checkTile = function(c, cls) {
          var tile = getTile(c.name);
          expect(tile).toBeOfClass("coordinator");
          expect(tile).toBeOfClass("btn-" + cls);
          expect(tile.children[0]).toBeOfClass("domino-header");
          expect($(tile.children[0]).text()).toEqual(c.name);
          expect($(tile.children[1]).text()).toEqual(c.url);
        };

      beforeEach(function() {
        view = new window.ClusterCoordinatorView();
        charly = {
          name: "Charly",
          url: "tcp://192.168.0.1:1337",
          status: "ok"
        };
        carlos = {
          name: "Carlos",
          url: "tcp://192.168.0.2:1337",
          status: "critical"
        };
        chantalle = {
          name: "Chantalle",
          url: "tcp://192.168.0.5:1337",
          status: "ok"
        };
        coordinators = [
          charly,
          carlos,
          chantalle
        ];
        view.fakeData.coordinators = coordinators;
        view.render();
      });

      it("should render all coordiniators", function() {
        expect($("> div.coordinator", $(div)).length).toEqual(3);
      });

      it("should render charly", function() {
        checkTile(charly, "success");
      });

      it("should render carlos", function() {
        checkTile(carlos, "danger");
      });

      it("should render chantalle", function() {
        checkTile(chantalle, "success");
      });

    });

  });

}());
