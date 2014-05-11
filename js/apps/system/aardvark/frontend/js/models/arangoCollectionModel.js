/*jslint indent: 2, nomen: true, maxlen: 120, vars: true, white: true, plusplus: true */
/*global require, window, Backbone, $, arangoHelper */
(function() {
  'use strict';
  window.arangoCollectionModel = Backbone.Model.extend({
    initialize: function () {
    },

    idAttribute: "name",

    urlRoot: "/_api/collection",
    defaults: {
      id: "",
      name: "",
      status: "",
      type: "",
      isSystem: false,
      picture: ""
    },

    getProperties: function () {
      var data2;
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_api/collection/" + encodeURIComponent(this.get("id")) + "/properties",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function(data) {
          data2 = data;
        },
        error: function(data) {
          data2 = data;
        }
      });
      return data2;
    },
    getFigures: function () {
      var data2;
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_api/collection/" + this.get("id") + "/figures",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function(data) {
          data2 = data;
        },
        error: function(data) {
          data2 = data;
        }
      });
      return data2;
    },
    getRevision: function () {
      var data2;
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_api/collection/" + this.get("id") + "/revision",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function(data) {
          data2 = data;
        },
        error: function(data) {
          data2 = data;
        }
      });
      return data2;
    },

    getIndex: function () {
      var data2;
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_api/index/?collection=" + this.get("id"),
        contentType: "application/json",
        processData: false,
        async: false,
        success: function(data) {
          data2 = data;
        },
        error: function(data) {
          data2 = data;
        }
      });
      return data2;
    },

  createIndex: function (postParameter) {
      var returnVal = false;

      $.ajax({
          cache: false,
          type: "POST",
          url: "/_api/index?collection="+ this.get("id"),
          data: JSON.stringify(postParameter),
          contentType: "application/json",
          processData: false,
          async: false,
          success: function(data) {
              returnVal = true;
          },
          error: function(data) {
              returnVal = data;
          }
      });
      return returnVal;
  },

      deleteIndex: function (id) {
          var returnval = false;
          $.ajax({
              cache: false,
              type: 'DELETE',
              url: "/_api/index/"+ this.get("name") +"/"+encodeURIComponent(id),
              async: false,
              success: function () {
                  returnval = true;
              },
              error: function () {
                  returnval = false;
              }
          });
          return returnval;
      },

    loadCollection: function () {
      var self = this;
      $.ajax({
        async:false,
        cache: false,
        type: 'PUT',
        url: "/_api/collection/" + this.get("id") + "/load",
        success: function () {
          self.set("status", "loaded");
          arangoHelper.arangoNotification('Collection loaded');
        },
        error: function () {
          arangoHelper.arangoError('Collection error');
        }
      });
    },

    unloadCollection: function () {
      var self = this;
      $.ajax({
        async:false,
        cache: false,
        type: 'PUT',
        url: "/_api/collection/" + this.get("id") + "/unload",
        success: function () {
          self.set("status", "unloaded");
          arangoHelper.arangoNotification('Collection unloaded');
        },
        error: function () {
          arangoHelper.arangoError('Collection error');
        }
      });
    },

    renameCollection: function (name) {
      var self = this,
        result = false;
      $.ajax({
        cache: false,
        type: "PUT",
        async: false, // sequential calls!
        url: "/_api/collection/" + this.get("id") + "/rename",
        data: '{"name":"' + name + '"}',
        contentType: "application/json",
        processData: false,
        success: function(data) {
          self.set("name", name);
          result = true;
        },
        error: function(data) {
          try {
            var parsed = JSON.parse(data.responseText);
            result = parsed.errorMessage;
          }
          catch (e) {
            result = false;
          }
        }
      });
      return result;
    },

    changeCollection: function (wfs, journalSize) {
      var result = false;

      $.ajax({
        cache: false,
        type: "PUT",
        async: false, // sequential calls!
        url: "/_api/collection/" + this.get("id") + "/properties",
        data: '{"waitForSync":' + wfs + ',"journalSize":' + JSON.stringify(journalSize) + '}',
        contentType: "application/json",
        processData: false,
        success: function(data) {
          result = true;
        },
        error: function(data) {
          try {
            var parsed = JSON.parse(data.responseText);
            result = parsed.errorMessage;
          }
          catch (e) {
            result = false;
          }
        }
      });
      return result;
    }

  });
}());
