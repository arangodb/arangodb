/* jshint browser: true */
/* jshint unused: false */
/* global window, Backbone */
(function () {
  'use strict';
  window.NotificationCollection = Backbone.Collection.extend({
    model: window.Notification,
    url: ''
  });
}());
