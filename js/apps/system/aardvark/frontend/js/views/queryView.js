/*jshint browser: true */
/*jshint unused: false */
/*global require, exports, Backbone, EJS, $, setTimeout, localStorage, ace, Storage, window, _ */
/*global _, arangoHelper, templateEngine, jQuery, Joi*/

(function () {
  "use strict";
  window.queryView = Backbone.View.extend({
    el: '#content',
    id: '#customsDiv',
    tabArray: [],

    initialize: function () {
      this.getAQL();
      this.getSystemQueries();
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
      'keypress #aqlEditor': 'aqlShortcuts',
      'click #arangoQueryTable .table-cell0': 'editCustomQuery',
      'click #arangoQueryTable .table-cell1': 'editCustomQuery',
      'click #arangoQueryTable .table-cell2 a': 'deleteAQL',
      'click #confirmQueryImport': 'importCustomQueries',
      'click #confirmQueryExport': 'exportCustomQueries',
      'click #downloadQueryResult': 'downloadQueryResult',
      'click #importQueriesToggle': 'showImportMenu'
    },

    showImportMenu: function(e) {
      $('#importQueriesToggle').toggleClass('activated');
      $('#importHeader').slideToggle(200);
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
          [
            {
              rule: Joi.string().required(),
              msg: "No query name given."
            }
          ]
        )
      );
      buttons.push(
        window.modalView.createSuccessButton('Save', this.saveAQL.bind(this))
      );
      window.modalView.show('modalTable.ejs', 'Save Query', buttons, tableContent, undefined,
        {'keyup #new-query-name' : this.listenKey.bind(this)});
    },

    updateTable: function () {
      this.tableDescription.rows = this.customQueries;

      _.each(this.tableDescription.rows, function(k, v) {
        k.thirdRow = '<a class="deleteButton"><span class="icon_arangodb_roundminus"' +
              ' title="Delete query"></span></a>';
      });

      // escape all columns but the third (which contains HTML)
      this.tableDescription.unescaped = [ false, false, true ];

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
        } 
        else {
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

      var sizeBox = $('#querySize');
      sizeBox.empty();
      [ 100, 250, 500, 1000, 2500, 5000 ].forEach(function (value) {
        sizeBox.append('<option value="' + _.escape(value) + '"' +
          (querySize === value ? ' selected' : '') +
          '>' + _.escape(value) + ' results</option>');
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

      this.initQueryImport();

      this.switchTab('query-switch');
      return this;
    },

    initQueryImport: function () {
      var self = this;
      self.allowUpload = false;
      $('#importQueries').change(function(e) {
        self.files = e.target.files || e.dataTransfer.files;
        self.file = self.files[0];

        self.allowUpload = true;
      });
    },

    importCustomQueries: function () {
      var result, self = this;
      if (this.allowUpload === true) {

        var callback = function() {
          this.collection.fetch({async: false});
          this.updateLocalQueries();
          this.renderSelectboxes();
          this.updateTable();
          self.allowUpload = false;
          $('#customs-switch').click();
        };

        self.collection.saveImportQueries(self.file, callback.bind(this));
      }
    },

    downloadQueryResult: function() {
      var inputEditor = ace.edit("aqlEditor");
      var query = inputEditor.getValue();
      if (query !== '' || query !== undefined || query !== null) {
        window.open(encodeURI("query/result/download/" + query));
      }
      else {
        arangoHelper.arangoError("Query error", "could not query result.");
      }
    },

    exportCustomQueries: function () {
      var name, toExport = {}, exportArray = [];
      _.each(this.customQueries, function(value, key) {
        exportArray.push({name: value.name, value: value.value});
      });
      toExport = {
        "extra": {
          "queries": exportArray
        }
      };

      $.ajax("whoAmI", {async:false}).done(
        function(data) {
        name = data.name;

        if (name === null) {
          name = "root";
        }

      });

      window.open(encodeURI("query/download/" + name));
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
      var self = this, result;

      this.collection.fetch({
        async: false
      });

      //old storage method
      if (localStorage.getItem("customQueries")) {

        var queries = JSON.parse(localStorage.getItem("customQueries"));
        //save queries in user collections extra attribute
        _.each(queries, function(oldQuery) {
          self.collection.add({
            value: oldQuery.value,
            name: oldQuery.name
          });
        });
        result = self.collection.saveCollectionQueries();

        if (result === true) {
          //and delete them from localStorage
          localStorage.removeItem("customQueries");
        }
      }

      this.updateLocalQueries();
    },

    deleteAQL: function (e) {
      var deleteName = $(e.target).parent().parent().parent().children().first().text();

      var toDelete = this.collection.findWhere({name: deleteName});
      this.collection.remove(toDelete);
      this.collection.saveCollectionQueries();

      this.updateLocalQueries();
      this.renderSelectboxes();
      this.updateTable();
    },

    updateLocalQueries: function () {
      var self = this;
      this.customQueries = [];

      this.collection.each(function(model) {
        self.customQueries.push({
          name: model.get("name"),
          value: model.get("value")
        });
      });
    },

    saveAQL: function (e) {
      e.stopPropagation();
      var inputEditor = ace.edit("aqlEditor");
      var saveName = $('#new-query-name').val();
      var isUpdate = $('#modalButton1').text() === 'Update';

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
          quit = !isUpdate;
          return;
        }
      });

      if (quit === true) {
        //Heiko: Form-Validator - name already taken
        window.modalView.hide();
        return;
      }

      if (! isUpdate) {
        //this.customQueries.push({
         // name: saveName,
         // value: content
        //});
        this.collection.add({
          name: saveName,
          value: content
        });
      }
      else {
        var toModifiy = this.collection.findWhere({name: saveName});
        toModifiy.set("value", content);
      }
      this.collection.saveCollectionQueries();

      window.modalView.hide();

      this.updateLocalQueries();
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

    renderSelectboxes: function () {
      this.sortQueries();
      var selector = '';
      selector = '#querySelect';
      $(selector).empty();

      $(selector).append('<option id="emptyquery">Insert Query</option>');

      $(selector).append('<optgroup label="Example queries">');
      jQuery.each(this.queries, function (k, v) {
        $(selector).append('<option id="' + _.escape(v.name) + '">' + _.escape(v.name) + '</option>');
      });
      $(selector).append('</optgroup>');

      if (this.customQueries.length > 0) {
        $(selector).append('<optgroup label="Custom queries">');
        jQuery.each(this.customQueries, function (k, v) {
          $(selector).append('<option id="' + _.escape(v.name) + '">' + _.escape(v.name) + '</option>');
        });
        $(selector).append('</optgroup>');
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

    abortQuery: function () {
    /*
      $.ajax({
        type: "DELETE",
        url: "/_api/cursor/currentFrontendQuery",
        contentType: "application/json",
        processData: false,
        success: function (data) {
        },
        error: function (data) {
        }
        });
    */
    },

    submitQuery: function () {
      var self = this;
      var inputEditor = ace.edit("aqlEditor");
      var selectedText = inputEditor.session.getTextRange(inputEditor.getSelectionRange());

      var sizeBox = $('#querySize');
      var data = {
        query: selectedText || inputEditor.getValue(),
        batchSize: parseInt(sizeBox.val(), 10),
        id: "currentFrontendQuery"
      };
      var outputEditor = ace.edit("queryOutput");

      // clear result
      outputEditor.setValue('');

      window.progressView.show(
        "Query is operating...",
        self.abortQuery("id"),
        '<button class="button-danger">Abort Query</button>'
      );

      $.ajax({
        type: "POST",
        url: "/_api/cursor",
        data: JSON.stringify(data),
        contentType: "application/json",
        processData: false,
        success: function (data) {
          outputEditor.setValue(JSON.stringify(data.result, undefined, 2));
          self.switchTab("result-switch");
          window.progressView.hide();
          self.deselect(outputEditor);
        },
        error: function (data) {
          self.switchTab("result-switch");
          try {
            var temp = JSON.parse(data.responseText);
            outputEditor.setValue('[' + temp.errorNum + '] ' + temp.errorMessage);
            arangoHelper.arangoError("Query error", temp.errorNum);
          }
          catch (e) {
            outputEditor.setValue('ERROR');
            arangoHelper.arangoError("Query error", "ERROR");
          }
          window.progressView.hide();
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
        if (element === switchId) {
          $("#" + element).parent().addClass("active");
          $(divId).addClass("active");
          $(contentDivId).show();
          if (switchId === 'query-switch') {
            // issue #1000: set focus to query input
            $('#aqlEditor .ace_text-input').focus();
          }
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
