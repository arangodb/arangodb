/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, localStorage, ace, Storage, window, arangoHelper*/

var queryView = Backbone.View.extend({
  el: '#content',
  initialize: function () {
    localStorage.setItem("queryContent", "");
    localStorage.setItem("queryOutput", "");
  },
  events: {
    'click #submitQueryIcon'   : 'submitQuery',
    'click #submitQueryButton' : 'submitQuery',
    'click .clearicon': 'clearOutput'
  },
  clearOutput: function() {
    $('#queryOutput').empty();
  },

  template: new EJS({url: 'js/templates/queryView.ejs'}),

  render: function() {
    var self = this;
    $(this.el).html(this.template.text);
    var editor = ace.edit("aqlEditor");
    var editor2 = ace.edit("queryOutput");

    editor2.setReadOnly(true);
    editor2.setHighlightActiveLine(false);

    editor.getSession().setMode("ace/mode/aql");
    editor2.getSession().setMode("ace/mode/json");
    editor.setTheme("ace/theme/merbivore_soft");
    editor2.setValue('');

    $('#queryOutput').resizable({
      handles: "s",
      ghost: true,
      stop: function () {
        setTimeout(function (){
          var editor2 = ace.edit("queryOutput");
          editor2.resize();
        },200);
      }
    });
    $('#aqlEditor').resizable({
      handles: "s",
      ghost: true,
      //helper: "resizable-helper",
      stop: function () {
        setTimeout(function (){
          var editor = ace.edit("aqlEditor");
          editor.resize();
        },200);
      }
    });

    $('#aqlEditor .ace_text-input').focus();
    $.gritter.removeAll();

    if(typeof Storage) {
      var queryContent = localStorage.getItem("queryContent");
      var queryOutput = localStorage.getItem("queryOutput");
      editor.setValue(queryContent);
      editor2.setValue(queryOutput);
    }

    var windowHeight = $(window).height() - 250;
    $('#queryOutput').height(windowHeight/2);
    $('#aqlEditor').height(windowHeight/2);

    editor.resize();
    editor2.resize();

    return this;
  },
  submitQuery: function() {
    var self = this;
    var editor = ace.edit("aqlEditor");
    var data = {query: editor.getValue()};

    var editor2 = ace.edit("queryOutput");

    $.ajax({
      type: "POST",
      url: "/_api/cursor",
      data: JSON.stringify(data),
      contentType: "application/json",
      processData: false,
      success: function(data) {
        editor2.setValue(arangoHelper.FormatJSON(data.result));
        if(typeof Storage) {
          localStorage.setItem("queryContent", editor.getValue());
          localStorage.setItem("queryOutput", editor2.getValue());
        }
      },
      error: function(data) {
        try {
          var temp = JSON.parse(data.responseText);
          editor2.setValue('[' + temp.errorNum + '] ' + temp.errorMessage);

          if(typeof Storage) {
            localStorage.setItem("queryContent", editor.getValue());
            localStorage.setItem("queryOutput", editor2.getValue());
          }
        }
        catch (e) {
          editor2.setValue('ERROR');
        }
      }
    });
    editor2.resize();

  }

});
