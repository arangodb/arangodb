/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global describe, it, expect, jasmine, spyOn*/
/*global $, d3*/

describe("Collection View", function() {
  "use strict";
  var myView;
  window.App = function() {};
  window.App.navigate = function() {};

  beforeEach(function() {
    $('body').append('<div id="content" class="removeMe"></div>');

    myView = new window.collectionView({
      model: arangoCollection
    });

  });

  afterEach(function() {
    $('.removeMe').remove();
  });


  describe("Collection Changes", function() {
    it("Check if changes were submitted", function() {

      var pressedEnter = false;
      myView.render();

      spyOn(myView, 'saveModifiedCollection').andCallFake(function(request) {
        pressedEnter = true;
        console.log("hasdhasd");
      });

      myView.saveModifiedCollection();
      expect(pressedEnter).toBeTruthy();
    });


  });
});

