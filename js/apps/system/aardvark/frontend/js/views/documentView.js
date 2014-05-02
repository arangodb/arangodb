/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true, forin: true */
/*global require, exports, Backbone, EJS, $, window, arangoHelper, jsoneditor, templateEngine */
/*global document */

(function() {
  "use strict";
  window.DocumentView = Backbone.View.extend({
    el: '#content',
    colid: 0,
    docid: 0,

    template: templateEngine.createTemplate("documentView.ejs"),

    events: {
      "click #saveDocumentButton" : "saveDocument"
    },

    editor: 0,

    typeCheck: function (type) {
      var result;
      if (type === 'edge') {
        result = window.arangoDocumentStore.getEdge(this.colid, this.docid);
      }
      else if (type === 'document') {
        result = window.arangoDocumentStore.getDocument(this.colid, this.docid);
      }
      if (result === true) {
        this.fillEditor();
      }
    },

    fillEditor: function() {
      var insert = this.collection.first().attributes;
      insert = this.removeReadonlyKeys(insert);
      this.editor.set(this.collection.first().attributes);
    },

    render: function() {
      $(this.el).html(this.template.render({}));
      this.breadcrumb();

      var container = document.getElementById('documentEditor');
      var options = {
        search: true,
        mode: 'tree',
        modes: ['tree', 'code']
      };
      this.editor = new jsoneditor.JSONEditor(container, options);

      return this;
    },

    removeReadonlyKeys: function (object) {
      delete object._key;
      delete object._rev;
      delete object._id;
      return object;
    },

    saveDocument: function () {
      var self = this;
      var model, result;
      model = this.editor.get();

      model = JSON.stringify(model);

      if (this.type === 'document') {
        result = window.arangoDocumentStore.saveDocument(this.colid, this.docid, model);
        if (result === false) {
          arangoHelper.arangoError('Document error:','Could not save');
          return;
        }
      }
      else if (this.type === 'edge') {
        result = window.arangoDocumentStore.saveEdge(this.colid, this.docid, model);
        if (result === false) {
          arangoHelper.arangoError('Edge error:', 'Could not save');
          return;
        }
      }

      if (result === true) {
        $('#showSaveState').fadeIn(1000).fadeOut(1000);
      }
    },

    breadcrumb: function () {
      var name = window.location.hash.split("/");
      $('#transparentHeader').append(
        '<div class="breadcrumb">'+
        '<a href="#collections" class="activeBread">Collections</a>'+
        '  >  '+
        '<a class="activeBread" href="#collection/' + name[1] + '/documents/1">' + name[1] + '</a>'+
        '  >  '+
        '<a class="disabledBread">' + name[2] + '</a>'+
        '</div>'
      );
    },

    getLinkedDoc: function (handle) {
      var self = this;
      if (! self.documentCache.hasOwnProperty(handle)) {
        $.ajax({
          cache: false,
          type: "GET",
          async: false,
          url: "/_api/document/" + handle,
          contentType: "application/json",
          processData: false,
          success: function(data) {
            self.documentCache[handle] = data;
          },
          error: function(data) {
            self.documentCache[handle] = null; 
          }
        });
      }

      return self.documentCache[handle];
    },

    escaped: function (value) {
      return value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;").replace(/'/g, "&#39;");
    }

  });
}());
