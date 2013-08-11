/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, window, arangoHelper, ace, arangoDocumentStore */

var documentSourceView = Backbone.View.extend({
  el: '#content',
  init: function () {
  },
  events: {
    "click #tableView"     :   "tableView",
    "click #saveSourceDoc" :   "saveSourceDoc"
  },

  template: new EJS({url: 'js/templates/documentSourceView.ejs'}),

  render: function() {
    $(this.el).html(this.template.text);
    this.breadcrumb();
    this.editor();
    return this;
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
      arangoHelper.arangoError('Unknown type: ' + type);
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
      '<a href="#" class="activeBread">Collections</a>'+
      '  >  '+
      '<a class="activeBread" href="#collection/'+name[1]+'/documents/1">'+name[1]+'</a>'+
      '  >  '+
      '<a class="disabledBread">'+name[2]+'</a>'+
      '</div>'
    );
  },
  saveSourceDoc: function() {
    var editor, model, result;
    if (this.type === 'document') {
      editor = ace.edit("sourceEditor");
      model = editor.getValue();
      result = window.arangoDocumentStore.saveDocument(this.colid, this.docid, model);
      if (result === true) {
        arangoHelper.arangoNotification('Document saved');
      }
      else if (result === false) {
        arangoHelper.arangoError('Document error');
      }
    }
    else if (this.type === 'edge') {
      editor = ace.edit("sourceEditor");
      model = editor.getValue();
      result = window.arangoDocumentStore.saveEdge(this.colid, this.docid, model);
      if (result === true) {
        arangoHelper.arangoNotification('Edge saved');
      }
      else if (result === false) {
        arangoHelper.arangoError('Edge error');
      }
    }
  },
  tableView: function () {
    var hash = window.location.hash.split("/");
    window.location.hash = hash[0]+"/"+hash[1]+"/"+hash[2];
  },
  fillSourceBox: function () {
    var data = arangoDocumentStore.models[0].attributes;
    var model = [];
    $.each(data, function(key, val) {
      if (arangoHelper.isSystemAttribute(key) === true) {
        delete data[key];
      }
    });
    var editor = ace.edit("sourceEditor");
    editor.setValue(arangoHelper.FormatJSON(data));
    editor.clearSelection();
  },
  stateReplace: function (value) {
    var inString = false;
    var length = value.length;
    var position = 0;
    var escaped = false;

    var output = "";
    while (position < length) {
      var c = value.charAt(position++);

      if (c === '\\') {
        if (escaped) {
          /* case: \ followed by \ */
          output += '\\\\';
          escaped = false;
        }
        else {
          /* case: single backslash */
          escaped = true;
        }
      }
      else if (c === '"') {
        if (escaped) {
          /* case: \ followed by " */
          output += '\\"';
          escaped = false;
        }
        else {
          output += '"';
          inString = !inString;
        }
      }
      else {
        if (inString) {
          if (escaped) {
            output += '\\' + c;
            escaped = false;
          }
          else {
            switch (c) {
              case '\b':
                output += '\\b';
              break;
              case '\f':
                output += '\\f';
              break;
              case '\n':
                output += '\\n';
              break;
              case '\t':
                output += '\\t';
              break;
              case '\r':
                output += '\\r';
              break;
              default:
                output += c;
              break;
            }
          }
        }
        else {
          if (c >= '!') {
            output += c;
          }
        }
      }
    }

    return output;
  }
});
