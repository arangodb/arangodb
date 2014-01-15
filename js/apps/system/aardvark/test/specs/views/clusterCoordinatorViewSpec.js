/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, */
/*global spyOn, expect*/
/*global templateEngine, $*/
(function() {
  "use strict";

  describe("Cluster Coordinator View", function() {
    var view, div;

    beforeEach(function() {
      div = document.createElement("div");
      div.id = "clusterServers"; 
      document.body.appendChild(div);
    });

    afterEach(function() {
      document.body.removeChild(div);
    });

    describe("rendering", function() {

      beforeEach(function() {
        view = new window.ClusterCoordinatorView();
      });

      it("should render the coordinators", function() {
        view.render();

      });

    });

  });

}());
