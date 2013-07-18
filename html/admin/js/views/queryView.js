/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, setTimeout, localStorage, ace, Storage, window*/
/*global arangoHelper*/

var queryView = Backbone.View.extend({
  el: '#content',

  initialize: function () {
    this.getAQL();
    localStorage.setItem("queryContent", "");
    localStorage.setItem("queryOutput", "");
  },

  events: {
    'click #submitQueryIcon'         : 'submitQuery',
    'click #submitQueryButton'       : 'submitQuery',
    'click #commentText'             : 'commentText',
    'click #undoText'                : 'undoText',
    'click #redoText'                : 'redoText',
    'click #smallOutput'             : 'smallOutput',
    'click #bigOutput'               : 'bigOutput',
    'click #clearOutput'             : 'clearOutput',
    'click #addAQL'                  : 'addAQL',
    'click #editAQL'                 : 'editAQL',
    'click #save-new-query'          : 'saveAQL',
    'click #save-edit-query'         : 'saveAQL',
    'click #delete-edit-query'       : 'showDeleteField',
    'click #confirmDeleteQuery'      : 'deleteAQL',
    'click #abortDeleteQuery'        : 'hideDeleteField',
    "keydown #new-query-name"        : "listenKey",
    'click #queryModalSelect option' : "updateEditSelect",
    'click #querySelect option'      : 'importSelected'
  },
  listenKey: function (e) {
    if (e.keyCode === 13) {
      this.saveAQL();
    }
  },

  clearOutput: function() {
    var editor2 = ace.edit("queryOutput");
    editor2.setValue('');
  },

  smallOutput: function() {
    var editor2 = ace.edit("queryOutput");
    editor2.getSession().foldAll();
  },

  bigOutput: function() {
    var editor2 = ace.edit("queryOutput");
    editor2.getSession().unfold();
  },

  queries: [
    {name: "TestA", value: "return [1,2,3,4,5]"},
    {name: "TestB", value: "return [6,7,8,9,10]"}
  ],

  customQueries: [],

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

    $('.queryTooltips').tooltip({
      placement: "top"
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
    $('#queryOutput').height(windowHeight/3);
    $('#aqlEditor').height(windowHeight/2);

    editor.resize();
    editor2.resize();

    this.renderSelectboxes();

    return this;
  },
  addAQL: function () {
    //render options
    $('#new-query-name').val('');
    $('#new-aql-query').modal('show');
    setTimeout(function (){
      $('#new-query-name').focus();
    },500);
  },
  editAQL: function () {
    if (this.customQueries.length === 0) {
      arangoHelper.arangoNotification("No custom queries available");
      return;
    }

    this.hideDeleteField();
    $('#edit-aql-queries').modal('show');
    this.renderSelectboxes(true);
    this.updateEditSelect();
  },
  getAQL: function () {
    if (localStorage.getItem("customQueries")) {
      this.customQueries = JSON.parse(localStorage.getItem("customQueries"));
    }
  },
  showDeleteField: function () {
    $('#reallyDeleteQueryDiv').show();
  },
  hideDeleteField: function () {
    $('#reallyDeleteQueryDiv').hide();
  },
  deleteAQL: function () {
    var queryName = $('#queryModalSelect').val();
    var tempArray = [];

    $.each(this.customQueries, function(k,v) {
      if (queryName !== v.name) {
        tempArray.push({
          name: v.name,
          value: v.value
        });
      }
    });

    this.customQueries = tempArray;
    localStorage.setItem("customQueries", JSON.stringify(this.customQueries));
    $('#edit-aql-queries').modal('hide');
    arangoHelper.arangoNotification("Query deleted");
    this.renderSelectboxes();
  },
  saveAQL: function (e) {

    var self = this;
    var editor = ace.edit("aqlEditor");
    var queryName = $('#new-query-name').val();
    var content = editor.getValue();

    if (e.target.id === 'save-edit-query') {
      content = $('#edit-aql-textarea').val();
      queryName = $('#queryModalSelect').val();
    }

    //check for already existing entry
    var tempArray = [];
    var quit = false;
    $.each(this.customQueries, function(k,v) {
      if (e.target.id !== 'save-edit-query') {
        if (v.name === queryName) {
          quit = true;
          return;
        }
      }
      if (v.name !== queryName) {
        tempArray.push({
          name: v.name,
          value: v.value
        });
      }
    });

    if (quit === true) {
      arangoHelper.arangoNotification("Name already taken!");
      return;
    }

    this.customQueries = tempArray;

    this.customQueries.push({
      name: queryName,
      value: content
    });

    $('#new-aql-query').modal('hide');
    $('#edit-aql-queries').modal('hide');

    localStorage.setItem("customQueries", JSON.stringify(this.customQueries));
    arangoHelper.arangoNotification("Query saved");
    this.renderSelectboxes();
  },
  updateEditSelect: function () {
    var value = this.getCustomQueryValueByName($('#queryModalSelect').val());
    $('#edit-aql-textarea').val(value);
    $('#edit-aql-textarea').focus();
  },
  getCustomQueryValueByName: function (qName) {
    var returnVal;
    $.each(this.customQueries, function(k,v) {
      if (qName === v.name) {
        returnVal = v.value;
      }
    });
    return returnVal;
  },
  importSelected: function(e) {
    var editor = ace.edit("aqlEditor");
    $.each(this.queries, function(k,v) {
      if (e.target.id === v.name) {
        editor.setValue(v.value);
      }
    });
    $.each(this.customQueries, function(k,v) {
      if (e.target.id === v.name) {
        editor.setValue(v.value);
      }
    });
  },
  renderSelectboxes: function (modal) {
    this.sortQueries();
    var selector = '';
    if (modal === true) {
      selector = '#queryModalSelect';
      $(selector).empty();
      $.each(this.customQueries, function(k,v) {
        $(selector).append('<option id="'+v.name+'">'+v.name+'</option>');
      });
    }
    else {
      selector = '#querySelect';
      $(selector).empty();
      $.each(this.queries, function(k,v) {
        $(selector).append('<option id="'+v.name+'">'+v.name+'</option>');
      });
      $.each(this.customQueries, function(k,v) {
        $(selector).append('<option id="'+v.name+'">'+v.name+'</option>');
      });
    }
  },
  undoText: function () {
    var editor = ace.edit("aqlEditor");
    editor.undo();
  },
  redoText: function () {
    var editor = ace.edit("aqlEditor");
    editor.redo();
  },
  commentText: function() {
    var editor = ace.edit("aqlEditor");
    var value;
    var newValue;
    var flag = false;
    var cursorPosition = editor.getCursorPosition();
    var cursorRange = editor.getSelection().getRange();

    var regExp = new RegExp(/\*\//);
    var regExp2 = new RegExp(/\/\*/);

    if (cursorRange.end.row === cursorRange.start.row) {
      //single line comment /* */
      value = editor.getSession().getLine(cursorRange.start.row);
      if (value.search(regExp) === -1 && value.search(regExp2) === -1) {
        newValue = '/*' + value + '*/';
        flag = true;
      }
      else if (value.search(regExp) !== -1 && value.search(regExp2) !== -1) {
        newValue = value.replace(regExp, '').replace(regExp2, '');
        flag = true;
      }
      if (flag === true) {
        editor.find(value, {
          range: cursorRange
        });
        editor.replace(newValue);
      }
    }
    else {
      //multi line comment
      value = editor.getSession().getLines(cursorRange.start.row, cursorRange.end.row);
      var arrayLength = value.length;
      var firstString = value[0];
      var lastString = value[value.length-1];
      var newFirstString;
      var newLastString;

      if (firstString.search(regExp2) === -1 && lastString.search(regExp) === -1) {
        newFirstString = '/*' + firstString;
        newLastString = lastString + '*/';
        flag = true;
      }
      else if (firstString.search(regExp2) !== -1 && lastString.search(regExp) !== -1) {
        newFirstString = firstString.replace(regExp2, '');
        newLastString = lastString.replace(regExp, '');
        flag = true;
      }
      if (flag === true) {
        editor.find(firstString, {
          range: cursorRange
        });
        editor.replace(newFirstString);
        editor.find(lastString, {
          range: cursorRange
        });
        editor.replace(newLastString);
      }
    }
    cursorRange.end.column = cursorRange.end.column + 2;
    editor.getSelection().setSelectionRange(cursorRange, false);
  },
  sortQueries: function() {
    this.queries = _.sortBy(this.queries, 'name' );
    this.customQueries = _.sortBy(this.customQueries, 'name' );
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
