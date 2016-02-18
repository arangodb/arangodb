/*global window, Backbone, $, arangoHelper */
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
      picture: "",
      locked: false,
      desc: undefined
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


    createIndex: function (postParameter, callback) {

      var self = this;

      $.ajax({
          cache: false,
          type: "POST",
          url: "/_api/index?collection="+ self.get("id"),
          headers: {
            'x-arango-async': 'store' 
          },
          data: JSON.stringify(postParameter),
          contentType: "application/json",
          processData: false,
          success: function (data, textStatus, xhr) {
            if (xhr.getResponseHeader('x-arango-async-id')) {
              window.arangoHelper.addAardvarkJob({
                id: xhr.getResponseHeader('x-arango-async-id'),
                type: 'index',
                desc: 'Creating Index',
                collection: self.get("id")
              });
              callback(false, data);
            }
            else {
              callback(true, data);
            }
          },
          error: function(data) {
            callback(true, data);
          }
      });
      callback();
    },

    deleteIndex: function (id, callback) {

      var self = this;

      $.ajax({
          cache: false,
          type: 'DELETE',
          url: "/_api/index/"+ this.get("name") +"/"+encodeURIComponent(id),
          headers: {
            'x-arango-async': 'store' 
          },
          success: function (data, textStatus, xhr) {
            if (xhr.getResponseHeader('x-arango-async-id')) {
              window.arangoHelper.addAardvarkJob({
                id: xhr.getResponseHeader('x-arango-async-id'),
                type: 'index',
                desc: 'Removing Index',
                collection: self.get("id")
              });
              callback(false, data);
            }
            else {
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
        async: false,
        cache: false,
        type: 'PUT',
        url: "/_api/collection/" + this.get("id") + "/truncate",
        success: function () {
          arangoHelper.arangoNotification('Collection truncated');
        },
        error: function () {
          arangoHelper.arangoError('Collection error');
        }
      });
    },

    loadCollection: function (callback) {

      $.ajax({
        cache: false,
        type: 'PUT',
        url: "/_api/collection/" + this.get("id") + "/load",
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
        url: "/_api/collection/" + this.get("id") + "/unload?flush=true",
        success: function () {
          callback(false);
        },
        error: function () {
          callback(true);
        }
      });
      callback();
    },

    renameCollection: function (name) {
      var self = this,
        result = false;
      $.ajax({
        cache: false,
        type: "PUT",
        async: false, // sequential calls!
        url: "/_api/collection/" + this.get("id") + "/rename",
        data: JSON.stringify({ name: name }),
        contentType: "application/json",
        processData: false,
        success: function() {
          self.set("name", name);
          result = true;
        },
        error: function(/*data*/) {
          try {
            console.log("error");
            //var parsed = JSON.parse(data.responseText);
            //result = parsed.errorMessage;
          }
          catch (e) {
            result = false;
          }
        }
      });
      return result;
    },

    changeCollection: function (wfs, journalSize, indexBuckets) {
      var result = false;
      if (wfs === "true") {
        wfs = true;
      }
      else if (wfs === "false") {
        wfs = false;
      }
      var data = {
        waitForSync: wfs,
        journalSize: parseInt(journalSize),
        indexBuckets: parseInt(indexBuckets)
      };

      $.ajax({
        cache: false,
        type: "PUT",
        async: false, // sequential calls!
        url: "/_api/collection/" + this.get("id") + "/properties",
        data: JSON.stringify(data),
        contentType: "application/json",
        processData: false,
        success: function() {
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
