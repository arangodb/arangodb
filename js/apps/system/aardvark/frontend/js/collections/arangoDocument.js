/*jshint browser: true */
/*jshint strict: false, unused: false */
/*global require, exports, Backbone, window, arangoDocument, arangoDocumentModel, $, arangoHelper */

window.arangoDocument = Backbone.Collection.extend({
  url: '/_api/document/',
  model: arangoDocumentModel,
  collectionInfo: {},
  deleteEdge: function (colid, docid) {
    var returnval = false;
    try {
      $.ajax({
        cache: false,
        type: 'DELETE',
        async: false,
        contentType: "application/json",
        url: "/_api/edge/" + colid + "/" + docid,
        success: function () {
          returnval = true;
        },
        error: function () {
          returnval = false;
        }
      });
    }
    catch (e) {
          returnval = false;
    }
    return returnval;
  },
  deleteDocument: function (colid, docid){
    var returnval = false;
    try {
      $.ajax({
        cache: false,
        type: 'DELETE',
        async: false,
        contentType: "application/json",
        url: "/_api/document/" + colid + "/" + docid,
        success: function () {
          returnval = true;
        },
        error: function () {
          returnval = false;
        }
      });
    }
    catch (e) {
          returnval = false;
    }
    return returnval;
  },
  addDocument: function (collectionID, key) {
  console.log("adding document");
    var self = this;
    self.createTypeDocument(collectionID, key);
  },
  createTypeEdge: function (collectionID, from, to, key) {
    var result = false, newEdge;

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
      async: false,
      url: "/_api/edge?collection=" + collectionID + "&from=" + from + "&to=" + to,
      data: newEdge,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        result = data._id;
      },
      error: function(data) {
        result = false;
      }
    });
    return result;
  },
  createTypeDocument: function (collectionID, key) {
    var result = false, newDocument;

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
      async: false,
      url: "/_api/document?collection=" + encodeURIComponent(collectionID),
      data: newDocument,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        result = data._id;
      },
      error: function(data) {
        result = false;
      }
    });
    return result;
  },
  getCollectionInfo: function (identifier) {
    var self = this;

    $.ajax({
      cache: false,
      type: "GET",
      url: "/_api/collection/" + identifier + "?" + arangoHelper.getRandomToken(),
      contentType: "application/json",
      processData: false,
      async: false,
      success: function(data) {
        self.collectionInfo = data;
      },
      error: function(data) {
      }
    });

    return self.collectionInfo;
  },
  getEdge: function (colid, docid){
    var result = false, self = this;
    this.clearDocument();
    $.ajax({
      cache: false,
      type: "GET",
      async: false,
      url: "/_api/edge/" + colid +"/"+ docid,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        self.add(data);
        result = true;
      },
      error: function(data) {
        result = false;
      }
    });
    return result;
  },
  getDocument: function (colid, docid) {
    var result = false, self = this;
    this.clearDocument();
    $.ajax({
      cache: false,
      type: "GET",
      async: false,
      url: "/_api/document/" + colid +"/"+ docid,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        self.add(data);
        result = true;
      },
      error: function(data) {
        result = false;
      }
    });
    return result;
  },
  saveEdge: function (colid, docid, model) {
    var result = false;
    $.ajax({
      cache: false,
      type: "PUT",
      async: false,
      url: "/_api/edge/" + colid + "/" + docid,
      data: model,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        result = true;
      },
      error: function(data) {
        result = false;
      }
    });
    return result;
  },
  saveDocument: function (colid, docid, model) {
    var result = false;
    $.ajax({
      cache: false,
      type: "PUT",
      async: false,
      url: "/_api/document/" + colid + "/" + docid,
      data: model,
      contentType: "application/json",
      processData: false,
      success: function(data) {
          result = true;
      },
      error: function(data) {
          result = false;
      }
    });
    return result;

  },

  updateLocalDocument: function (data) {
    this.clearDocument();
    this.add(data);
  },
  clearDocument: function () {
    this.reset();
  }

});
