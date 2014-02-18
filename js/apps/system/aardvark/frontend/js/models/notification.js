/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, Backbone */
(function() {
  "use strict";

  window.Notification = Backbone.Model.extend({
    defaults: {
      "title"    : "",
      "date"     : 0,
      "content"  : "",
      "priority" : "",
      "tags"     : "",
      "seen"     : false
    }

  });
}());
