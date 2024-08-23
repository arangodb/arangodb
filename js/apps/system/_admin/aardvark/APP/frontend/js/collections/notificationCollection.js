(function () {
  'use strict';
  window.NotificationCollection = Backbone.Collection.extend({
    model: window.Notification,
    url: ''
  });
}());
