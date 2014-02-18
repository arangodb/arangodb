/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, window*/
/*global arangoHelper, ace, arangoDocumentStore, templateEngine, _ */

(function() {
  "use strict";
  window.DocumentSourceView = Backbone.View.extend({
    el: '#content',
    init: function () {
    },
    events: {
      "click #tableView"       : "tableView",
      "click #saveSourceDoc"   : "saveSourceDoc",
      "keypress #sourceEditor" : "sourceShortcut"
    },

    template: templateEngine.createTemplate("documentSourceView.ejs"),

    render: function() {
      $(this.el).html(this.template.text);
      this.breadcrumb();
      this.editor();

      $('#sourceEditor').resizable({
        handles: "s",
        ghost: true,
        stop: function () {
          window.setTimeout(function () {
            var editor = ace.edit("sourceEditor");
            editor.resize();
          },200);
        }
      });
      return this;
    },

    sourceShortcut: function (e) {
      if (e.ctrlKey && e.keyCode === 13) {
        this.saveSourceDoc();
      }
      else if (e.metaKey && !e.ctrlKey && e.keyCode === 13) {
        this.saveSourceDoc();
      }
    },

    typeCheck: function (type) {
      this.type = type;
      if (type === 'edge') {
        window.arangoDocumentStore.getEdge(this.colid, this.docid);
        window.documentSourceView.fillSourceBox();
      }
      else if (type === 'document') {
        window.arangoDocumentStore.getDocument(this.colid, this.docid);
        window.documentSourceView.fillSourceBox();
      }
      else {
        arangoHelper.arangoError('Unknown document type: ' + type);
      }
    },
    editor: function () {
      var editor = ace.edit("sourceEditor");
      editor.getSession().setMode("ace/mode/javascript");
    },
    breadcrumb: function () {
      var name = window.location.hash.split("/");
      $('#transparentHeader').append(
        '<div class="breadcrumb">'+
        '<a href="#collections" class="activeBread">Collections</a>'+
        '  >  '+
        '<a class="activeBread" href="#collection/'+name[1]+'/documents/1">'+name[1]+'</a>'+
        '  >  '+
        '<a class="disabledBread">'+name[2]+'</a>'+
        '</div>'
      );
    },
    saveSourceDoc: function() {
      var editor, model, parsed, result;
      if (this.type === 'document') {
        editor = ace.edit("sourceEditor");
        model = editor.getValue();
     
        try {
          parsed = JSON.parse(model); 
        }
        catch (err) {
          arangoHelper.arangoError('Invalid document');
          return;
        } 

        if (arangoDocumentStore.models[0].internalAttributeChanged(parsed)) {
          arangoHelper.arangoError('Cannot change internal attributes of a document');
          return;
        }

        result = window.arangoDocumentStore.saveDocument(this.colid, this.docid, model);
        if (result === false) {
          arangoHelper.arangoError('Document error');
        }
      }
      else if (this.type === 'edge') {
        editor = ace.edit("sourceEditor");
        model = editor.getValue();
        result = window.arangoDocumentStore.saveEdge(this.colid, this.docid, model);
        if (result === false) {
          arangoHelper.arangoError('Edge error');
        }
      }
    },
    tableView: function () {
      var hash = window.location.hash.split("/");
      window.location.hash = hash[0] + "/" + hash[1] + "/" + hash[2];
    },
    fillSourceBox: function () {
      var editor = ace.edit("sourceEditor");
      editor.setValue(arangoHelper.FormatJSON(arangoDocumentStore.models[0].getSorted()));
      editor.clearSelection();
    }
  });
}());
