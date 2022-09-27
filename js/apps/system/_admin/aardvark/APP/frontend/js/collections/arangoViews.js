/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, window, arangoViewModel, arangoHelper, _ */
(function () {
  'use strict';

  window.ArangoViews = Backbone.Collection.extend({
    url: arangoHelper.databaseUrl('/_api/view'),

    model: arangoViewModel,

    parse: function (response) {
      return _.filter(response.result, {type: 'arangosearch'});
    }

    /*
    newIndex: function (object, callback) {
    }
    */

  });
}());
