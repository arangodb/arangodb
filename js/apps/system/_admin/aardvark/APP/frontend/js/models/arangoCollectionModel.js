/* global window, Backbone, $, arangoHelper */
(function () {
  'use strict';
  window.arangoCollectionModel = Backbone.Model.extend({
    idAttribute: 'name',

    urlRoot: arangoHelper.databaseUrl('/_api/collection'),
    defaults: {
      id: '',
      name: '',
      status: '',
      type: '',
      isSystem: false,
      picture: '',
      locked: false,
      desc: undefined
    },

    getProperties: function (callback) {
      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/collection/' + encodeURIComponent(this.get('id')) + '/properties'),
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
    getFigures: function (callback) {
      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/collection/' + this.get('id') + '/figures'),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          callback(false, data);
        },
        error: function () {
          callback(true);
        }
      });
    },
    getRevision: function (callback, figures) {
      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/collection/' + this.get('id') + '/revision'),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          callback(false, data, figures);
        },
        error: function () {
          callback(true);
        }
      });
    },

    getIndex: function (callback) {
      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/index/?collection=' + this.get('id')),
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

    createIndex: function (postParameter, callback) {
      var self = this;

      $.ajax({
        cache: false,
        type: 'POST',
        url: arangoHelper.databaseUrl('/_api/index?collection=' + self.get('id')),
        headers: {
          'x-arango-async': 'store'
        },
        data: JSON.stringify(postParameter),
        contentType: 'application/json',
        processData: false,
        success: function (data, textStatus, xhr) {
          if (xhr.getResponseHeader('x-arango-async-id')) {
            window.arangoHelper.addAardvarkJob({
              id: xhr.getResponseHeader('x-arango-async-id'),
              type: 'index',
              desc: 'Creating Index',
              collection: self.get('id')
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

    deleteIndex: function (id, callback) {
      var self = this;

      $.ajax({
        cache: false,
        type: 'DELETE',
        url: arangoHelper.databaseUrl('/_api/index/' + this.get('name') + '/' + encodeURIComponent(id)),
        headers: {
          'x-arango-async': 'store'
        },
        success: function (data, textStatus, xhr) {
          if (xhr.getResponseHeader('x-arango-async-id')) {
            window.arangoHelper.addAardvarkJob({
              id: xhr.getResponseHeader('x-arango-async-id'),
              type: 'index',
              desc: 'Removing Index',
              collection: self.get('id')
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
      callback();
    },

    truncateCollection: function () {
      $.ajax({
        cache: false,
        type: 'PUT',
        url: arangoHelper.databaseUrl('/_api/collection/' + this.get('id') + '/truncate'),
        success: function () {
          arangoHelper.arangoNotification('Collection truncated.');
        },
        error: function () {
          arangoHelper.arangoError('Collection error.');
        }
      });
    },

    loadCollection: function (callback) {
      $.ajax({
        cache: false,
        type: 'PUT',
        url: arangoHelper.databaseUrl('/_api/collection/' + this.get('id') + '/load'),
        success: function () {
          callback(false);
        },
        error: function () {
          callback(true);
        }
      });
      callback();
    },

    unloadCollection: function (callback) {
      $.ajax({
        cache: false,
        type: 'PUT',
        url: arangoHelper.databaseUrl('/_api/collection/' + this.get('id') + '/unload?flush=true'),
        success: function () {
          callback(false);
        },
        error: function () {
          callback(true);
        }
      });
      callback();
    },

    renameCollection: function (name, callback) {
      var self = this;

      $.ajax({
        cache: false,
        type: 'PUT',
        url: arangoHelper.databaseUrl('/_api/collection/' + this.get('id') + '/rename'),
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
    },

    changeCollection: function (wfs, journalSize, indexBuckets, callback) {
      var result = false;
      if (wfs === 'true') {
        wfs = true;
      } else if (wfs === 'false') {
        wfs = false;
      }
      var data = {
        waitForSync: wfs,
        journalSize: parseInt(journalSize, 10),
        indexBuckets: parseInt(indexBuckets, 10)
      };

      $.ajax({
        cache: false,
        type: 'PUT',
        url: arangoHelper.databaseUrl('/_api/collection/' + this.get('id') + '/properties'),
        data: JSON.stringify(data),
        contentType: 'application/json',
        processData: false,
        success: function () {
          callback(false);
        },
        error: function (data) {
          callback(false, data);
        }
      });
      return result;
    }

  });
}());
