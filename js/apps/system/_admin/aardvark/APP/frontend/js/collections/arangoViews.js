/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, window, arangoViewModel, arangoHelper */
(function () {
  'use strict';

  window.ArangoViews = Backbone.Collection.extend({
    url: arangoHelper.databaseUrl('/_api/view'),

    model: arangoViewModel,

    parse: function (response) {
      return response.result;
    }

    /*
    newIndex: function (object, callback) {
    }
    */

  });
}());
