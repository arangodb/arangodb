/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global $*/

(function() {
  "use strict";

  describe("The notification view", function() {

    var view, div, fakeNotification, dummyCollection, jQueryDummy;

    beforeEach(function() {
      div = document.createElement("div");
      div.id = "navigationBar";
      document.body.appendChild(div);

      dummyCollection = new window.NotificationCollection();

      fakeNotification = {
        title: "Hallo",
        date: 1398239658,
        content: "",
        priority: "",
        tags: "",
        seen: false
      };

      view = new window.NotificationView({
        collection: dummyCollection
      });

      view.render();
    });

    afterEach(function() {
      document.body.removeChild(div);
    });

    it("assert basics", function () {
      expect(view.events).toEqual({
        "click .navlogo #stat_hd" : "toggleNotification",
        "click .notificationItem .fa" : "removeNotification",
        "click #removeAllNotifications" : "removeAllNotifications"
      });
    });

    it("toggleNotification should run function", function () {
      jQueryDummy = {
        toggle: function () {
        }
      };
      spyOn(jQueryDummy, "toggle");
      spyOn(window, "$").andReturn(
        jQueryDummy
      );
      view.toggleNotification();
      expect(jQueryDummy.toggle).not.toHaveBeenCalled();
    });

    it("toggleNotification should run function", function () {
      view.collection.add(fakeNotification);
      jQueryDummy = {
        toggle: function () {
        }
      };
      spyOn(jQueryDummy, "toggle");
      spyOn(window, "$").andReturn(
        jQueryDummy
      );
      view.toggleNotification();
      expect(jQueryDummy.toggle).toHaveBeenCalled();
    });

    it("toggleNotification should run function", function () {
      spyOn(view.collection, "reset");
      view.removeAllNotifications();
      expect(view.collection.reset).toHaveBeenCalled();
      expect($('#notification_menu').is(":visible")).toBeFalsy();
    });




  });


}());
