/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, */
/*global spyOn, expect, runs, waitsFor*/
/*global templateEngine, $, uiMatchers*/
(function() {
  "use strict";

  describe("Shutdown Button View", function() {
    var div;

    beforeEach(function() {
      div = document.createElement("div");
      div.id = "navigationBar";
      document.body.appendChild(div);

    });

    it("should bind the overview", function() {
      var overview = {
        id: "overview"
      },
      view = new window.ShutdownButtonView({
        overview: overview
      });
      expect(view.overview).toEqual(overview);
    });

    describe("rendered", function() {

      var view;

      beforeEach(function() {
        var overview = {
          stopUpdating: function() {
            throw "This is should be a spy";
          }
        };
        spyOn(overview.stopUpdating);
        view = new window.ShutdownButtonView({
          overview: overview
        });
        view.render();
      });

      it("should be able to shutdown the cluster", function() {
        spyOn($, "ajax").andCallFake(function(opt) {
        
        });
        spyOn(window, "$").andCallFake(function(obj) {
          
        });
        $("#clusterShutdown").click();

      });

      it("should be able to unrender", function() {
        expect($(div).html()).not.toEqual("");
        view.unrender();
        expect($(div).html()).toEqual("");
      });


    });

  });
