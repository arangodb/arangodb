/* jshint browser: true */
/* jshint strict: false, unused: false */
/* global Backbone, window, arangoDocumentModel, $, arangoHelper */

window.ArangoDocument = Backbone.Collection.extend({
  url: '/_api/document/',
  model: arangoDocumentModel,
  collectionInfo: {},

  deleteEdge: function (colid, dockey, callback) {
    this.deleteDocument(colid, dockey, callback);
  },

  deleteDocument: function (colid, dockey, callback) {
    var data = {
      keys: [dockey],
      collection: colid
    };

    $.ajax({
      cache: false,
      type: 'PUT',
      contentType: 'application/json',
      data: JSON.stringify(data),
      url: arangoHelper.databaseUrl('/_api/simple/remove-by-keys'),
      success: function () {
        callback(false);
      },
      error: function () {
        callback(true);
      }
    });
  },

  addDocument: function (collectionID, key, body) {
    this.createTypeDocument(collectionID, key, body);
  },

  createTypeEdge: function (collectionID, from, to, key, body, callback) {
    var newEdge;

    try {
      if (key) {
        body._key = key;
      }
      body._from = from;
      body._to = to;
      newEdge = JSON.stringify(body);
    }
    catch (x) {
      body = {};
      return this.createTypeEdge(collectionID, from, to, key, body, callback);
    }

    $.ajax({
      cache: false,
      type: 'POST',
      url: arangoHelper.databaseUrl('/_api/document?collection=' + encodeURIComponent(collectionID)),
      data: newEdge,
      contentType: 'application/json',
      processData: false,
      success: function (data) {
        callback(false, data._id);
      },
      error: function (data) {
        callback(true, data._id, data.responseJSON);
      }
    });
  },

  createTypeDocument: function (collectionID, key, body, callback, returnNew,
                                smartJoinAttribute, smartJoinAttributeValue,
                                smartGraphAttribute, smartGraphAttributeValue) {
    var newDocument = body;

    try {
      if (smartJoinAttribute && smartJoinAttributeValue && key) {
        // case: smartJoin, bot value are needed and NOT optional
        newDocument._key = smartJoinAttributeValue + ':' + key;
        newDocument[smartJoinAttribute] = smartJoinAttributeValue;
      } else if (smartGraphAttribute && smartGraphAttributeValue) {
        // case: smartGraph with value
        // other to smartJoin, we can:
        // 1.) Create without smartGraphAttribute and without smartGraphAttributeValue
        // 2.) Create only with smartGraphAttributeValue
        if (key) {
          newDocument._key = smartGraphAttributeValue + ':' + key;
        }
        newDocument[smartGraphAttribute] = smartGraphAttributeValue;
      } else if (key) {
        newDocument._key = key;
      }
    }
    catch (x) {
      body = {};
      return this.createTypeDocument(collectionID, key, body, callback, returnNew,
                                     smartJoinAttribute, smartJoinAttributeValue,
                                     smartGraphAttribute, smartGraphAttributeValue);
    }
    newDocument = JSON.stringify(newDocument);

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
      url: arangoHelper.databaseUrl('/_api/collection/' + encodeURIComponent(identifier) + '?' + arangoHelper.getRandomToken()),
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

  getEdge: function (colid, dockey, callback) {
    this.getDocument(colid, dockey, callback);
  },

  getDocument: function (colid, dockey, callback) {
    var self = this;
    this.clearDocument();

    var data = {
      keys: [dockey],
      collection: colid
    };

    $.ajax({
      cache: false,
      type: 'PUT',
      url: arangoHelper.databaseUrl('/_api/simple/lookup-by-keys'),
      contentType: 'application/json',
      data: JSON.stringify(data),
      processData: false,
      success: function (data) {
        if (data.documents[0]) {
          self.add(data.documents[0]);
          if (data.documents[0]._from && data.documents[0]._to) {
            callback(false, data, 'edge');
          } else {
            callback(false, data, 'document');
          }
        } else {
          self.add(true, data);
          callback(true, data, colid + '/' + dockey);
        }
      },
      error: function (data) {
        self.add(true, data);
        arangoHelper.arangoError('Error', data.responseJSON.errorMessage + ' - error number: ' + data.responseJSON.errorNum);
        callback(true, data, colid + '/' + dockey);
      }
    });
  },

  saveEdge: function (colid, dockey, from, to, model, callback) {
    this.saveDocument(colid, dockey, model, callback, from, to);
  },

  saveDocument: function (colid, dockey, model, callback, from, to) {
    if (typeof model === 'string') {
      try {
        model = JSON.parse(model);
      } catch (e) {
        arangoHelper.arangoError('Could not parse document: ' + model);
      }
    }
    model._key = dockey;
    model._id = colid + '/' + dockey;

    if (from && to) {
      model._from = from;
      model._to = to;
    }

    $.ajax({
      cache: false,
      type: 'PUT',
      url: arangoHelper.databaseUrl('/_api/document/' + encodeURIComponent(colid) + '?returnNew=true'),
      data: JSON.stringify([model]),
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
