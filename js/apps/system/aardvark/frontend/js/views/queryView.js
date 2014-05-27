/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, setTimeout, localStorage, ace, Storage, window, _ */
/*global arangoHelper, templateEngine*/

(function () {
  "use strict";
  window.queryView = Backbone.View.extend({
    el: '#content',
    id: '#customsDiv',
    tabArray: [],

    initialize: function () {
      this.getAQL();
      this.getSystemQueries();
      localStorage.setItem("queryContent", "");
      localStorage.setItem("queryOutput", "");
      this.tableDescription.rows = this.customQueries;
    },

    events: {
      "click #result-switch": "switchTab",
      "click #query-switch": "switchTab",
      'click #customs-switch': "switchTab",
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
      'change #querySelect': 'importSelected',
      'change #querySize': 'changeSize',
      'keypress #aqlEditor': 'aqlShortcuts',
      'click #arangoQueryTable .table-cell0': 'editCustomQuery',
      'click #arangoQueryTable .table-cell1': 'editCustomQuery',
      'click #arangoQueryTable .table-cell2 a': 'deleteAQL',
      'click #queryDiv .showHotkeyHelp' : 'shortcutModal'

    },

    shortcutModal: function() {
      window.arangoHelper.hotkeysFunctions.showHotkeysModal();
    },

    createCustomQueryModal: function(){
      var buttons = [], tableContent = [];
      tableContent.push(
        window.modalView.createTextEntry(
          'new-query-name',
          'Name',
          '',
          undefined,
          undefined,
          false,
          /[<>&'"]/
        )
      );
      buttons.push(
        window.modalView.createSuccessButton('Save', this.saveAQL.bind(this))
      );
      window.modalView.show('modalTable.ejs', 'Save Query', buttons, tableContent, undefined,
        {'keyup #new-query-name' : this.listenKey.bind(this)});
    },

    updateTable:  function () {
      this.tableDescription.rows = this.customQueries;

      _.each(this.tableDescription.rows, function(k,v) {
        k.thirdRow = '<a class="deleteButton"><span class="icon_arangodb_roundminus"' +
              ' title="Delete query"></span></a>';
      });

      this.$(this.id).html(this.table.render({content: this.tableDescription}));
    },

    editCustomQuery: function(e) {
      var queryName = $(e.target).parent().children().first().text();
      var inputEditor = ace.edit("aqlEditor");
      inputEditor.setValue(this.getCustomQueryValueByName(queryName));
      this.deselect(inputEditor);
      $('#querySelect').val(queryName);
      this.switchTab("query-switch");
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
      this.checkSaveName();
    },

    checkSaveName: function() {
      var saveName = $('#new-query-name').val();
      if ( saveName === "Insert Query"){
        $('#new-query-name').val('');
        return;
      }

      //check for invalid query names, if present change the box-shadow to red
      // and disable the save functionality
      var boolTemp = false;
      this.customQueries.some(function(query){
        if( query.name === saveName ){
          $('#modalButton1').removeClass('button-success');
          $('#modalButton1').addClass('button-warning');
          $('#modalButton1').text('Update');
            boolTemp = true;
        } else {
          $('#modalButton1').removeClass('button-warning');
          $('#modalButton1').addClass('button-success');
          $('#modalButton1').text('Save');
        }

        if (boolTemp) {
          return true;
        }
      });
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
    },

    queries: [
    ],

    customQueries: [],


    tableDescription: {
      id: "arangoQueryTable",
      titles: ["Name", "Content", ""],
      rows: []
    },

    template: templateEngine.createTemplate("queryView.ejs"),
    table: templateEngine.createTemplate("arangoTable.ejs"),

    render: function () {
      this.$el.html(this.template.render({}));
      this.$(this.id).html(this.table.render({content: this.tableDescription}));
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

      var inputEditor = ace.edit("aqlEditor");
      inputEditor.getSession().setMode("ace/mode/aql");
      inputEditor.setFontSize("16px");
      inputEditor.setOptions({fontFamily: "Courier New"});
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
      $('#aqlEditor').height(windowHeight - 19);
      $('#queryOutput').height(windowHeight);

      inputEditor.resize();
      outputEditor.resize();

      this.initTabArray();
      this.renderSelectboxes();
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
      this.createCustomQueryModal();
      $('#new-query-name').val($('#querySelect').val());
      setTimeout(function () {
        $('#new-query-name').focus();
      }, 500);
      this.checkSaveName();
    },

    getAQL: function () {
      if (localStorage.getItem("customQueries")) {
        this.customQueries = JSON.parse(localStorage.getItem("customQueries"));
      }
    },

    deleteAQL: function (e) {

      var deleteName = $(e.target).parent().parent().parent().children().first().text();
      var tempArray = [];

      $.each(this.customQueries, function (k, v) {
        if (deleteName !== v.name) {
          tempArray.push({
            name: v.name,
            value: v.value
          });
        }
      });

      this.customQueries = tempArray;
      localStorage.setItem("customQueries", JSON.stringify(this.customQueries));
      this.renderSelectboxes();
      this.updateTable();
    },

    saveAQL: function (e) {
      e.stopPropagation();
      var inputEditor = ace.edit("aqlEditor");
      var saveName = $('#new-query-name').val();

      if ($('#new-query-name').hasClass('invalid-input')) {
        return;
      }

      //Heiko: Form-Validator - illegal query name
      if (saveName.trim() === '') {
        return;
      }

      var content = inputEditor.getValue();

      //check for already existing entry
      var quit = false;
      $.each(this.customQueries, function (k, v) {
          if (v.name === saveName) {
            v.value = content;
            quit = true;
            return;
          }
        });

      if (quit === true) {
        //Heiko: Form-Validator - name already taken
        window.modalView.hide();
        return;
      }

      this.customQueries.push({
        name: saveName,
        value: content
      });

      window.modalView.hide();

      localStorage.setItem("customQueries", JSON.stringify(this.customQueries));
      this.renderSelectboxes();
      $('#querySelect').val(saveName);
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
      var switchId = typeof e === 'string' ? e : e.target.id;
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
      this.updateTable();
    }
  });
}());
