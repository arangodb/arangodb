/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/

(function() {
  "use strict";

  describe("The new logs view", function() {

    var view;
    var dummyCollection = {
      fetch: function() {
      }
    };

      beforeEach(function() {


      view = new window.NewLogsView({
          logall: dummyCollection
      });

    });

    it("set active log level", function () {

      var dummyCollection = {
        fetch: function() {
        }
      };

      spyOn(view, "collection").andReturn(dummyCollection);
      spyOn(dummyCollection, "fetch").andReturn(dummyCollection.fetch());

      var element = {
        currentTarget: {
          id: "loginfo"
        }
      };

      spyOn(view, "convertModelToJSON");
      view.setActiveLoglevel(element);
      expect(view.convertModelToJSON).toHaveBeenCalled();
    });


  });


}());
