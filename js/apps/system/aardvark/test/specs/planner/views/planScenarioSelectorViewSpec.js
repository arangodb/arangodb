/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global runs, waitsFor, jasmine*/
/*global $*/
(function() {
  "use strict";

  describe("PlanScenarioSelector View", function() {

  var myView;

    beforeEach(function() {
        $('body').append('<div id="content" class="removeMe"></div>');
        window.App = {
            navigate: function() {}
        };
        spyOn(window.App, "navigate");
        myView = new window.PlanScenarioSelectorView();
        myView.render();

    });

    afterEach(function() {
      $('.removeMe').remove();
    });


    describe("select scenario", function() {
        it("multiServerSymmetrical", function() {
            $("#multiServerSymmetrical").click();
            expect(App.navigate).toHaveBeenCalledWith("planSymmetrical", {trigger: true});

        });

        it("multiServerAsymmetrical", function() {
          $("#multiServerAsymmetrical").click();
          expect(App.navigate).toHaveBeenCalledWith("planAsymmetrical", {trigger: true});

        });
        it("singleServer", function() {
          $("#singleServer").click();
          expect(App.navigate).toHaveBeenCalledWith("planTest", {trigger: true});
        });
    });
  });
}());
