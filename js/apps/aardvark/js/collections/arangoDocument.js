/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, window, arangoDocument, $, arangoHelper */
window.arangoDocument = Backbone.Collection.extend({
  url: '/_api/document/',
  model: arangoDocument,
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
  addDocument: function (collectionID) {
    var self = this;
    self.createTypeDocument(collectionID);
  },
  createTypeEdge: function (collectionID, from, to) {
    var result = false;
    $.ajax({
      cache: false,
      type: "POST",
      async: false,
      url: "/_api/edge?collection=" + collectionID + "&from=" + from + "&to=" + to,
      data: JSON.stringify({}),
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
  createTypeDocument: function (collectionID) {
    var result = false;
    $.ajax({
      cache: false,
      type: "POST",
      async: false,
      url: "/_api/document?collection=" + collectionID,
      data: JSON.stringify({}),
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
    var result = false;
    this.clearDocument();
    $.ajax({
      cache: false,
      type: "GET",
      async: false,
      url: "/_api/edge/" + colid +"/"+ docid,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        window.arangoDocumentStore.add(data);
        result = true;
      },
      error: function(data) {
          result = false;
      }
    });
    return result;
  },
  getDocument: function (colid, docid) {
    var result = false;
    this.clearDocument();
    $.ajax({
      cache: false,
      type: "GET",
      async: false,
      url: "/_api/document/" + colid +"/"+ docid,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        window.arangoDocumentStore.add(data);
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
          //window.location.hash = "collection/"+collid+"/documents/1";
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
          //window.location.hash = "collection/"+collid+"/documents/1";
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
    window.arangoDocumentStore.reset();
  }

});
