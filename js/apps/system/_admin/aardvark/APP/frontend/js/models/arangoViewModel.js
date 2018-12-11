/* global window, Backbone, $, arangoHelper */
(function () {
  'use strict';
  window.arangoViewModel = Backbone.Model.extend({
    idAttribute: 'name',

    urlRoot: arangoHelper.databaseUrl('/_api/view'),
    defaults: {
      id: '',
      name: '',
      type: '',
      globallyUniqueId: ''
    },

    getProperties: function (callback) {
      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/view/' + encodeURIComponent(this.get('name')) + '/properties'),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          callback(false, data);
        },
        error: function (data) {
          callback(true, data);
        }
      });
    },

    patchProperties: function (reqData, callback) {
      var self = this;

      $.ajax({
        type: 'PUT',
        cache: false,
        data: JSON.stringify(reqData),
        url: arangoHelper.databaseUrl('/_api/view/' + encodeURIComponent(this.get('name')) + '/properties'),
        headers: {
          'x-arango-async': 'store'
        },
        contentType: 'application/json',
        processData: false,
        success: function (data, textStatus, xhr) {
          if (xhr.getResponseHeader('x-arango-async-id')) {
            window.arangoHelper.addAardvarkJob({
              id: xhr.getResponseHeader('x-arango-async-id'),
              type: 'view',
              desc: 'Patching View',
              collection: self.get('name')
            });
            callback(false, data, true);
          } else {
            callback(true, data);
          }
          callback(false, data, false);
        }
      });
    },

    deleteView: function (callback) {
      var self = this;

      $.ajax({
        cache: false,
        type: 'DELETE',
        url: arangoHelper.databaseUrl('/_api/view/' + self.get('name')),
        headers: {
          'x-arango-async': 'store'
        },
        success: function (data, textStatus, xhr) {
          if (xhr.getResponseHeader('x-arango-async-id')) {
            window.arangoHelper.addAardvarkJob({
              id: xhr.getResponseHeader('x-arango-async-id'),
              type: 'view',
              desc: 'Removing View',
              collection: self.get('name')
            });
            callback(false, data);
          } else {
            callback(true, data);
          }
        },
        error: function (data) {
          callback(true, data);
        }
      });
    },

    renameView: function (name, callback) {
      var self = this;

      $.ajax({
        cache: false,
        type: 'PUT',
        url: arangoHelper.databaseUrl('/_api/view/' + self.get('name') + '/rename'),
        data: JSON.stringify({ name: name }),
        contentType: 'application/json',
        processData: false,
        success: function () {
          self.set('name', name);
          callback(false);
        },
        error: function (data) {
          callback(true, data);
        }
      });
    }

  });
}());
