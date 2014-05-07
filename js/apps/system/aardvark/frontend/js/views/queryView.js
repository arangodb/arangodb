/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, setTimeout, localStorage, ace, Storage, window, _ */
/*global arangoHelper, templateEngine*/

(function () {
  "use strict";
  window.queryView = Backbone.View.extend({
    el: '#content',
    tabArray: [],

    initialize: function () {
      this.getAQL();
      this.getSystemQueries();
      localStorage.setItem("queryContent", "");
      localStorage.setItem("queryOutput", "");
    },

    events: {
      "click #result-switch": "switchTab",
      "click #query-switch": "switchTab",
      'click #customs-switch': 'switchTab',
      'click #submitQueryIcon': 'submitQuery',
      'click #submitQueryButton': 'submitQuery',
      'click #commentText': 'commentText',
      'click #uncommentText': 'uncommentText',
      'click #undoText': 'undoText',
      'click #redoText': 'redoText',
      'click #smallOutput': 'smallOutput',
      'click #bigOutput': 'bigOutput',
      'click #clearOutput': 'clearOutput',
      'click #clearInput': 'clearInput',
      'click #clearQueryButton': 'clearInput',
      'click #addAQL': 'addAQL',
      'click #editAQL': 'editAQL',
      'click #save-new-query': 'saveAQL',
      'click #save-edit-query': 'saveAQL',
      'click #delete-edit-query': 'showDeleteField',
      'click #confirmDeleteQuery': 'deleteAQL',
      'click #abortDeleteQuery': 'hideDeleteField',
      'keydown #new-query-name': 'listenKey',
      'change #queryModalSelect': 'updateEditSelect',
      'change #querySelect': 'importSelected',
      'change #querySize': 'changeSize',
      'keypress #aqlEditor': 'aqlShortcuts'
    },

    initTabArray: function() {
      var self = this;
      $(".arango-tab").children().each( function(index) {
        self.tabArray.push($(this).children().first().attr("id"));
      });
    },

    listenKey: function (e) {
      if (e.keyCode === 13) {
        this.saveAQL(e);
      }
    },

    clearOutput: function () {
      var outputEditor = ace.edit("queryOutput");
      outputEditor.setValue('');
    },

    clearInput: function () {
      var inputEditor = ace.edit("aqlEditor");
      inputEditor.setValue('');
    },

    smallOutput: function () {
      var outputEditor = ace.edit("queryOutput");
      outputEditor.getSession().foldAll();
    },

    bigOutput: function () {
      var outputEditor = ace.edit("queryOutput");
      outputEditor.getSession().unfold();
    },

    aqlShortcuts: function (e) {
      if (e.ctrlKey && e.keyCode === 13) {
        this.submitQuery();
      }
      else if (e.metaKey && !e.ctrlKey && e.keyCode === 13) {
        this.submitQuery();
      }
      else if (e.ctrlKey && e.keyCode === 90) {
        // TO_DO: undo/redo seems to work even without this. check if can be removed
        this.undoText();
      }
      else if (e.ctrlKey && e.shiftKey && e.keyCode === 90) {
        // TO_DO: undo/redo seems to work even without this. check if can be removed
        this.redoText();
      }
    },

    queries: [
    ],

    customQueries: [],

    template: templateEngine.createTemplate("queryView.ejs"),

    render: function () {
      this.$el.html(this.template.render({}));

      // fill select box with # of results
      var querySize = 1000;
      if (typeof Storage) {
        if (localStorage.getItem("querySize") > 0) {
          querySize = parseInt(localStorage.getItem("querySize"), 10);
        }
      }

      var sizeBox = $('#querySize');
      sizeBox.empty();
      [ 100, 250, 500, 1000, 2500, 5000 ].forEach(function (value) {
        sizeBox.append('<option value="' + value + '"' +
          (querySize === value ? ' selected' : '') +
          '>' + value + ' results</option>');
      });

      var outputEditor = ace.edit("queryOutput");
      outputEditor.setReadOnly(true);
      outputEditor.setHighlightActiveLine(false);
      outputEditor.getSession().setMode("ace/mode/json");
      outputEditor.setFontSize("16px");
      outputEditor.setValue('');


      var customsEditor = ace.edit("customsEditor");
      customsEditor.getSession().setMode("ace/mode/aql");
      customsEditor.setFontSize("16px");
      customsEditor.commands.addCommand({
        name: "togglecomment",
        bindKey: {win: "Ctrl-Shift-C", linux: "Ctrl-Shift-C", mac: "Command-Shift-C"},
        exec: function (editor) {
          editor.toggleCommentLines();
        },
        multiSelectAction: "forEach"
      });

      customsEditor.getSession().selection.on('changeCursor', function (e) {
        var customsEditor = ace.edit("customsEditor");
        var session = customsEditor.getSession();
        var cursor = customsEditor.getCursorPosition();
        var token = session.getTokenAt(cursor.row, cursor.column);
        if (token) {
          if (token.type === "comment") {
            $("#commentText i")
            .removeClass("fa-comment")
            .addClass("fa-comment-o")
            .attr("data-original-title", "Uncomment");
          } else {
            $("#commentText i")
            .removeClass("fa-comment-o")
            .addClass("fa-comment")
            .attr("data-original-title", "Comment");
          }
        }
      });

      var inputEditor = ace.edit("aqlEditor");
      inputEditor.getSession().setMode("ace/mode/aql");
      inputEditor.setFontSize("16px");
      inputEditor.commands.addCommand({
        name: "togglecomment",
        bindKey: {win: "Ctrl-Shift-C", linux: "Ctrl-Shift-C", mac: "Command-Shift-C"},
        exec: function (editor) {
          editor.toggleCommentLines();
        },
        multiSelectAction: "forEach"
      });

      inputEditor.getSession().selection.on('changeCursor', function (e) {
        var inputEditor = ace.edit("aqlEditor");
        var session = inputEditor.getSession();
        var cursor = inputEditor.getCursorPosition();
        var token = session.getTokenAt(cursor.row, cursor.column);
        if (token) {
          if (token.type === "comment") {
            $("#commentText i")
            .removeClass("fa-comment")
            .addClass("fa-comment-o")
            .attr("data-original-title", "Uncomment");
          } else {
            $("#commentText i")
            .removeClass("fa-comment-o")
            .addClass("fa-comment")
            .attr("data-original-title", "Comment");
          }
        }
      });
      $('#queryOutput').resizable({
        handles: "s",
        ghost: true,
        stop: function () {
          setTimeout(function () {
            var outputEditor = ace.edit("queryOutput");
            outputEditor.resize();
          }, 200);
        }
      });

      arangoHelper.fixTooltips(".queryTooltips, .icon_arangodb", "top");

      $('#aqlEditor .ace_text-input').focus();

      if (typeof Storage) {
        var queryContent = localStorage.getItem("queryContent");
        var queryOutput = localStorage.getItem("queryOutput");
        inputEditor.setValue(queryContent);
        outputEditor.setValue(queryOutput);
      }

      var windowHeight = $(window).height() - 295;
      $('#customsEditor').height(windowHeight);
      $('#aqlEditor').height(windowHeight - 19);
      $('#queryOutput').height(windowHeight);

      customsEditor.resize();
      inputEditor.resize();
      outputEditor.resize();

      this.initTabArray();
      this.renderSelectboxes();
      this.deselect(customsEditor);
      this.deselect(outputEditor);
      this.deselect(inputEditor);

      // Max: why do we need to tell those elements to show themselves?
      $("#queryDiv").show();
      $("#customsDiv").show();

      this.switchTab('query-switch');

      return this;
    },

    deselect: function (editor) {
      var current = editor.getSelection();
      var currentRow = current.lead.row;
      var currentColumn = current.lead.column;

      current.setSelectionRange({
        start: {
          row: currentRow,
          column: currentColumn
        },
        end: {
          row: currentRow,
          column: currentColumn
        }
      });

      editor.focus();
    },

    addAQL: function () {
      //render options
      $('#new-query-name').val('');
      $('#new-aql-query').modal('show');
      setTimeout(function () {
        $('#new-query-name').focus();
      }, 500);
    },
    editAQL: function () {
      if (this.customQueries.length === 0) {
        //Heiko: display information that no custom queries are available
        return;
      }

      this.hideDeleteField();
      $('#queryDropdown').slideToggle();
      //$('#edit-aql-queries').modal('show');
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

      $.each(this.customQueries, function (k, v) {
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
      this.renderSelectboxes();
    },
    saveAQL: function (e) {
      var inputEditor = ace.edit("aqlEditor");
      var queryName = $('#new-query-name').val();
      var content = inputEditor.getValue();

      if (e) {
        if (e.target.id === 'save-edit-query') {
          content = $('#edit-aql-textarea').val();
          queryName = $('#queryModalSelect').val();
        }
      }

      if (queryName.trim() === '') {
        //Heiko: Form-Validator - illegal query name
        return;
      }

      //check for already existing entry
      var tempArray = [];
      var quit = false;
      $.each(this.customQueries, function (k, v) {
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
        //Heiko: Form-Validator - name already taken
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
      this.renderSelectboxes();
    },
    updateEditSelect: function () {
      var value = this.getCustomQueryValueByName($('#queryModalSelect').val());
      $('#edit-aql-textarea').val(value);
      $('#edit-aql-textarea').focus();
    },
    getSystemQueries: function () {
      var self = this;
      $.ajax({
        type: "GET",
        cache: false,
        url: "js/arango/aqltemplates.json",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function (data) {
          self.queries = data;
        },
        error: function (data) {
          arangoHelper.arangoNotification("Query", "Error while loading system templates");
        }
      });
    },
    getCustomQueryValueByName: function (qName) {
      var returnVal;
      $.each(this.customQueries, function (k, v) {
        if (qName === v.name) {
          returnVal = v.value;
        }
      });
      return returnVal;
    },
    importSelected: function (e) {
      var inputEditor = ace.edit("aqlEditor");
      $.each(this.queries, function (k, v) {
        if ($('#' + e.currentTarget.id).val() === v.name) {
          inputEditor.setValue(v.value);
        }
      });
      $.each(this.customQueries, function (k, v) {
        if ($('#' + e.currentTarget.id).val() === v.name) {
          inputEditor.setValue(v.value);
        }
      });

      this.deselect(ace.edit("aqlEditor"));
    },
    changeSize: function (e) {
      if (Storage) {
        localStorage.setItem("querySize", parseInt($('#' + e.currentTarget.id).val(), 10));
      }
    },
    renderSelectboxes: function (modal) {
      this.sortQueries();
      var selector = '';
      if (modal === true) {
        selector = '#queryModalSelect';
        $(selector).empty();
        $.each(this.customQueries, function (k, v) {
          var escapedName = arangoHelper.escapeHtml(v.name);
          $(selector).append('<option id="' + escapedName + '">' + escapedName + '</option>');
        });
      }
      else {
        selector = '#querySelect';
        $(selector).empty();

        $(selector).append('<option id="emptyquery">Insert Query</option>');

        $(selector).append('<optgroup label="Example queries">');
        $.each(this.queries, function (k, v) {
          var escapedName = arangoHelper.escapeHtml(v.name);
          $(selector).append('<option id="' + escapedName + '">' + escapedName + '</option>');
        });
        $(selector).append('</optgroup>');

        if (this.customQueries.length > 0) {
          $(selector).append('<optgroup label="Custom queries">');
          $.each(this.customQueries, function (k, v) {
            var escapedName = arangoHelper.escapeHtml(v.name);
            $(selector).append('<option id="' + escapedName + '">' + escapedName + '</option>');
          });
          $(selector).append('</optgroup>');
        }
      }
    },
    undoText: function () {
      var inputEditor = ace.edit("aqlEditor");
      inputEditor.undo();
    },
    redoText: function () {
      var inputEditor = ace.edit("aqlEditor");
      inputEditor.redo();
    },
    commentText: function () {
      var inputEditor = ace.edit("aqlEditor");
      inputEditor.toggleCommentLines();
    },
    sortQueries: function () {
      this.queries = _.sortBy(this.queries, 'name');
      this.customQueries = _.sortBy(this.customQueries, 'name');
    },
    submitQuery: function () {
      this.switchTab("result-switch");
      var self = this;
      var sizeBox = $('#querySize');
      var inputEditor = ace.edit("aqlEditor");
      var data = {
        query: inputEditor.getValue(),
        batchSize: parseInt(sizeBox.val(), 10)
      };
      var outputEditor = ace.edit("queryOutput");

      $.ajax({
        type: "POST",
        url: "/_api/cursor",
        data: JSON.stringify(data),
        contentType: "application/json",
        processData: false,
        success: function (data) {
          outputEditor.setValue(JSON.stringify(data.result, undefined, 2));
          if (typeof Storage) {
            localStorage.setItem("queryContent", inputEditor.getValue());
            localStorage.setItem("queryOutput", outputEditor.getValue());
          }
          self.deselect(outputEditor);
        },
        error: function (data) {
          try {
            var temp = JSON.parse(data.responseText);
            outputEditor.setValue('[' + temp.errorNum + '] ' + temp.errorMessage);

            if (typeof Storage) {
              localStorage.setItem("queryContent", inputEditor.getValue());
              localStorage.setItem("queryOutput", outputEditor.getValue());
            }
          }
          catch (e) {
            outputEditor.setValue('ERROR');
          }
        }
      });
      outputEditor.resize();
      this.deselect(inputEditor);

    },

    // This function changes the focus onto the tab that has been clicked
    // it can be given an event-object or the id of the tab to switch to
    //    e.g. switchTab("result-switch");
    // note that you need to ommit the #
    switchTab: function (e) {
      // defining a callback function for Array.forEach() the tabArray holds the ids of
      // the tabs a-tags, from which we can create the appropriate content-divs ids.
      // The convention is #result-switch (a-tag), #result (content-div), and
      // #tabContentResult (pane-div).
      // We set the clicked element's tags to active/show and the others to hide.
      //console.log("Type of e: " + typeof e + "e: " + e);
      var switchId = typeof e === 'string' ? e : e.target.id;
      //console.log("switchId: " + switchId);
      var changeTab = function (element, index, array){
        var divId = "#" + element.replace("-switch", "");
        var contentDivId = "#tabContent" + divId.charAt(1).toUpperCase() + divId.substr(2);
        if ( element === switchId){
          $("#" + element).parent().addClass("active");
          $(divId).addClass("active");
          $(contentDivId).show();
        } else {
          $("#" + element).parent().removeClass("active");
          $(divId).removeClass("active");
          $(contentDivId).hide();
        }
      };
      this.tabArray.forEach(changeTab);
    }
  });
}());
