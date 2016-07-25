/* jshint browser: true */
/* jshint unused: false */
/* global window, Backbone, arangoHelper */
(function () {
  'use strict';
  window.CoordinatorCollection = Backbone.Collection.extend({
    model: window.Coordinator,

    url: arangoHelper.databaseUrl('/_admin/aardvark/cluster/Coordinators')

  });
}());
