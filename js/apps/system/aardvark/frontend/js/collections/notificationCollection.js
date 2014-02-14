/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true */
/*global window, Backbone */
(function() {
  "use strict";
  window.NotificationCollection = Backbone.Collection.extend({
    model: window.Notification,
    url: ""
  });
}());
