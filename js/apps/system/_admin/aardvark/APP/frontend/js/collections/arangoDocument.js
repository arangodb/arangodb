/*jshint browser: true */
/*jshint strict: false, unused: false */
/*global Backbone, window, arangoDocument, arangoDocumentModel, $, arangoHelper */

window.arangoDocument = Backbone.Collection.extend({
  url: '/_api/document/',
  model: arangoDocumentModel,
  collectionInfo: {},
  deleteEdge: function (colid, docid, callback) {
    $.ajax({
      cache: false,
      type: 'DELETE',
      contentType: "application/json",
      url: "/_api/edge/" + colid + "/" + docid,
      success: function () {
        callback(false);
      },
      error: function () {
        callback(true);
      }
    });
  },
  deleteDocument: function (colid, docid, callback) {
    $.ajax({
      cache: false,
      type: 'DELETE',
      contentType: "application/json",
      url: "/_api/document/" + colid + "/" + docid,
      success: function () {
        callback(false);
      },
      error: function () {
        callback(true);
      }
    });
  },
  addDocument: function (collectionID, key) {
    var self = this;
    self.createTypeDocument(collectionID, key);
  },
  createTypeEdge: function (collectionID, from, to, key, callback) {
    var newEdge;

    if (key) {
      newEdge = JSON.stringify({
        _key: key
      });
    }
    else {
      newEdge = JSON.stringify({});
    }

    $.ajax({
      cache: false,
      type: "POST",
      url: "/_api/edge?collection=" + collectionID + "&from=" + from + "&to=" + to,
      data: newEdge,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        callback(false, data);
      },
      error: function(data) {
        callback(true, data);
      }
    });
  },
  createTypeDocument: function (collectionID, key, callback) {
    var newDocument;

    if (key) {
      newDocument = JSON.stringify({
        _key: key
      });
    }
    else {
      newDocument = JSON.stringify({});
    }

    $.ajax({
      cache: false,
      type: "POST",
      url: "/_api/document?collection=" + encodeURIComponent(collectionID),
      data: newDocument,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        callback(false, data._id);
      },
      error: function(data) {
        callback(true, data._id);
      }
    });
  },
  getCollectionInfo: function (identifier, callback, toRun) {
    var self = this;

    $.ajax({
      cache: false,
      type: "GET",
      url: "/_api/collection/" + identifier + "?" + arangoHelper.getRandomToken(),
      contentType: "application/json",
      processData: false,
      success: function(data) {
        self.collectionInfo = data;
        callback(false, data, toRun);
      },
      error: function(data) {
        callback(true, data, toRun);
      }
    });
  },
  getEdge: function (colid, docid, callback){
    var self = this;
    this.clearDocument();
    $.ajax({
      cache: false,
      type: "GET",
      url: "/_api/edge/" + colid +"/"+ docid,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        self.add(data);
        callback(false, data, 'edge');
      },
      error: function(data) {
        callback(true, data);
      }
    });
  },
  getDocument: function (colid, docid, callback) {
    var self = this;
    this.clearDocument();
    $.ajax({
      cache: false,
      type: "GET",
      url: "/_api/document/" + colid +"/"+ docid,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        self.add(data);
        callback(false, data, 'document');
      },
      error: function(data) {
        self.add(true, data);
      }
    });
  },
  saveEdge: function (colid, docid, model, callback) {
    $.ajax({
      cache: false,
      type: "PUT",
      url: "/_api/edge/" + colid + "/" + docid,
      data: model,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        callback(false, data);
      },
      error: function(data) {
        callback(true, data);
      }
    });
  },
  saveDocument: function (colid, docid, model, callback) {
    $.ajax({
      cache: false,
      type: "PUT",
      url: "/_api/document/" + colid + "/" + docid,
      data: model,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        callback(false, data);
      },
      error: function(data) {
        callback(true, data);
      }
    });
  },

  updateLocalDocument: function (data) {
    this.clearDocument();
    this.add(data);
  },
  clearDocument: function () {
    this.reset();
  }

});
