/* jshint browser: true */
/* jshint strict: false, unused: false */
/* global Backbone, window, arangoDocumentModel, $, arangoHelper */

window.ArangoDocument = Backbone.Collection.extend({
  url: '/_api/document/',
  model: arangoDocumentModel,
  collectionInfo: {},

  deleteEdge: function (colid, docid, callback) {
    this.deleteDocument(colid, docid, callback);
  },

  deleteDocument: function (colid, docid, callback) {
    $.ajax({
      cache: false,
      type: 'DELETE',
      contentType: 'application/json',
      url: arangoHelper.databaseUrl('/_api/document/' + encodeURIComponent(colid) + '/' + encodeURIComponent(docid)),
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
        _key: key,
        _from: from,
        _to: to
      });
    } else {
      newEdge = JSON.stringify({
        _from: from,
        _to: to
      });
    }

    $.ajax({
      cache: false,
      type: 'POST',
      url: arangoHelper.databaseUrl('/_api/document?collection=' + encodeURIComponent(collectionID)),
      data: newEdge,
      contentType: 'application/json',
      processData: false,
      success: function (data) {
        callback(false, data);
      },
      error: function (data) {
        callback(true, data._id, data.responseJSON);
      }
    });
  },
  createTypeDocument: function (collectionID, key, callback, returnNew) {
    var newDocument;

    if (key) {
      newDocument = JSON.stringify({
        _key: key
      });
    } else {
      newDocument = JSON.stringify({});
    }

    var url = arangoHelper.databaseUrl('/_api/document?collection=' + encodeURIComponent(collectionID));

    if (returnNew) {
      url = url + '?returnNew=true';
    }

    $.ajax({
      cache: false,
      type: 'POST',
      url: url,
      data: newDocument,
      contentType: 'application/json',
      processData: false,
      success: function (data) {
        if (returnNew) {
          callback(false, data);
        } else {
          callback(false, data._id);
        }
      },
      error: function (data) {
        callback(true, data._id, data.responseJSON);
      }
    });
  },
  getCollectionInfo: function (identifier, callback, toRun) {
    var self = this;

    $.ajax({
      cache: false,
      type: 'GET',
      url: arangoHelper.databaseUrl('/_api/collection/' + identifier + '?' + arangoHelper.getRandomToken()),
      contentType: 'application/json',
      processData: false,
      success: function (data) {
        self.collectionInfo = data;
        callback(false, data, toRun);
      },
      error: function (data) {
        callback(true, data, toRun);
      }
    });
  },
  getEdge: function (colid, docid, callback) {
    this.getDocument(colid, docid, callback);
  },
  getDocument: function (colid, docid, callback) {
    var self = this;
    this.clearDocument();
    $.ajax({
      cache: false,
      type: 'GET',
      url: arangoHelper.databaseUrl('/_api/document/' + encodeURIComponent(colid) + '/' + encodeURIComponent(docid)),
      contentType: 'application/json',
      processData: false,
      success: function (data) {
        self.add(data);
        callback(false, data, 'document');
      },
      error: function (data) {
        arangoHelper.arangoError('Error', data.responseJSON.errorMessage + ' - error number: ' + data.responseJSON.errorNum);
        self.add(true, data);
      }
    });
  },

  saveEdge: function (colid, docid, from, to, model, callback) {
    var parsed;
    try {
      parsed = JSON.parse(model);
      parsed._to = to;
      parsed._from = from;
    } catch (e) {
      arangoHelper.arangoError('Edge', 'Could not parsed document.');
    }

    $.ajax({
      cache: false,
      type: 'PUT',
      url: arangoHelper.databaseUrl('/_api/document/' + encodeURIComponent(colid) + '/' + encodeURIComponent(docid)) + '#replaceEdge',
      data: JSON.stringify(parsed),
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

  saveDocument: function (colid, docid, model, callback) {
    $.ajax({
      cache: false,
      type: 'PUT',
      url: arangoHelper.databaseUrl('/_api/document/' + encodeURIComponent(colid) + '/' + encodeURIComponent(docid)),
      data: model,
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

  updateLocalDocument: function (data) {
    this.clearDocument();
    this.add(data);
  },
  clearDocument: function () {
    this.reset();
  }

});
