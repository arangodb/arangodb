/* global window, Backbone, arangoHelper */
(function () {
  'use strict';
  window.ArangoQuery = Backbone.Model.extend({
    urlRoot: arangoHelper.databaseUrl('/_api/user'),

    defaults: {
      name: '',
      type: 'custom',
      value: ''
    }

  });
}());
