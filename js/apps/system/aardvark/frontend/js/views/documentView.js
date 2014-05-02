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
        result = this.collection.getEdge(this.colid, this.docid);
      }
      else if (type === 'document') {
        result = this.collection.getDocument(this.colid, this.docid);
      }
      if (result === true) {
        this.fillEditor();
        return true;
      }
    },

    fillEditor: function() {
      var toFill = this.removeReadonlyKeys(this.collection.first().attributes);
      this.editor.set(toFill);
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
      this.editor = new window.jsoneditor.JSONEditor(container, options);

      return this;
    },

    removeReadonlyKeys: function (object) {
      delete object._key;
      delete object._rev;
      delete object._id;
      delete object._from;
      delete object._to;
      return object;
    },

    saveDocument: function () {
      var model, result;
      model = this.editor.get();

      model = JSON.stringify(model);

      if (this.type === 'document') {
        result = this.collection.saveDocument(this.colid, this.docid, model);
        if (result === false) {
          arangoHelper.arangoError('Document error:','Could not save');
          return;
        }
      }
      else if (this.type === 'edge') {
        result = this.collection.saveEdge(this.colid, this.docid, model);
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

    escaped: function (value) {
      return value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;").replace(/'/g, "&#39;");
    }

  });
}());
