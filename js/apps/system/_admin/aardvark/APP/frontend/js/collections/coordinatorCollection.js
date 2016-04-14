/*jshint browser: true */
/*jshint unused: false */
/*global window, Backbone, $ */
(function() {
  "use strict";
  window.CoordinatorCollection = Backbone.Collection.extend({
    model: window.Coordinator,

    url: "/_admin/aardvark/cluster/Coordinators"

  });
}());
