/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/

(function() {
  "use strict";

  describe("The new logs view", function() {

    var view;

    beforeEach(function() {

      view = new window.NewLogsView();

    });

    it("set active log level", function () {

      var dummyCollection = {
        fetch: function() {
        }
      };

      spyOn(this, "activeCollection").andReturn(dummyCollection);
      spyOn(dummyCollection, "fetch").andReturn(dummyCollection.fetch());

      var element = {
        currentTarget: {
          id: "loginfo"
        }
      };

      spyOn(view, "convertModelToJSON");
      view.setActiveLogLevel(element);
      expect(view.convertModelToJSON).toHaveBeenCalled();
    });


  });


}());
