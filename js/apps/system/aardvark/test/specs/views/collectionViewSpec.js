/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global runs, waitsFor, jasmine*/
/*global $, arangoCollection*/
(function() {
  "use strict";

  describe("Collection View", function() {

    var myView, oldRouter;


    beforeEach(function() {
      oldRouter = window.App;
      window.App = function() {};
      window.App.navigate = function() {};
      $('body').append('<div id="content" class="removeMe"></div>');

      myView = new window.CollectionView({
        model: arangoCollection
      });

    });

    afterEach(function() {
      $('.removeMe').remove();
      window.App = oldRouter;
    });


    describe("Collection Changes", function() {
      it("Check if changes were submitted", function() {

        var pressedEnter = false;
        myView.render();

        spyOn(myView, 'saveModifiedCollection').andCallFake(function(request) {
          pressedEnter = true;
        });

        myView.saveModifiedCollection();
        expect(pressedEnter).toBeTruthy();
      });


    });
  });
}());
