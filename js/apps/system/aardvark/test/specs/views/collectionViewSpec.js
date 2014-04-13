/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global runs, waitsFor, jasmine*/
/*global $, arangoCollectionModel*/
(function() {
  "use strict";

  describe("Collection View", function() {

    var myView, isCoordinator;


    beforeEach(function() {
      isCoordinator = false;
      window.App = {
        navigate: function() {
          throw "This should be a spy";
        }
      };
      spyOn(window.App, "navigate");
      spyOn(window, "isCoordinator").andReturn(isCoordinator);
      $('body').append('<div id="content" class="removeMe"></div>');

      myView = new window.CollectionView({
        model: arangoCollectionModel
      });

    });

    afterEach(function() {
      $('.removeMe').remove();
      delete window.App;
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
