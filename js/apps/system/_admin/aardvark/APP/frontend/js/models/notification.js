/* global window, Backbone */
(function () {
  'use strict';

  window.Notification = Backbone.Model.extend({
    defaults: {
      'title': '',
      'date': 0,
      'content': '',
      'priority': '',
      'tags': '',
      'seen': false
    }

  });
}());
