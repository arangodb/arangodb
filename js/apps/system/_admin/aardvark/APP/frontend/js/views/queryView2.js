/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, EJS, $, setTimeout, localStorage, ace, Storage, window, _, console, btoa*/
/*global _, arangoHelper, templateEngine, jQuery, Joi, d3*/

(function () {
  "use strict";
  window.queryView2 = Backbone.View.extend({
    el: '#content',
    bindParamId: '#bindParamEditor',
    myQueriesId: '#queryTable',
    template: templateEngine.createTemplate("queryView2.ejs"),
    table: templateEngine.createTemplate("arangoTable.ejs"),

    outputDiv: '#outputEditors',
    outputTemplate: templateEngine.createTemplate("queryViewOutput.ejs"),
    outputCounter: 0,

    allowUpload: false,

    customQueries: [],
    queries: [],

    currentQuery: {},
    initDone: false,

    bindParamRegExp: /@(@?\w+\d*)/,
    bindParamTableObj: {},

    bindParamTableDesc: {
      id: "arangoBindParamTable",
      titles: ["Key", "Value"],
      rows: []
    },

    myQueriesTableDesc: {
      id: "arangoMyQueriesTable",
      titles: ["Name", "Query", "Actions"],
      rows: []
    },

    execPending: false,

    aqlEditor: null,
    queryPreview: null,

    initialize: function () {
      this.refreshAQL();
    },

    allowParamToggle: true,

    events: {
      "click #executeQuery": "executeQuery",
      "click #explainQuery": "explainQuery",
      "click #clearQuery": "clearQuery",
      'click .outputEditorWrapper #downloadQueryResult': 'downloadQueryResult',
      'click .outputEditorWrapper .switchAce': 'switchAce',
      "click .outputEditorWrapper .fa-close": "closeResult",
      "click #toggleQueries1": "toggleQueries",
      "click #toggleQueries2": "toggleQueries",
      "click #saveCurrentQuery": "addAQL",
      "click #exportQuery": "exportCustomQueries",
      "click #importQuery": "openImportDialog",
      "click #removeResults": "removeResults",
      "click #querySpotlight": "showSpotlight",
      "click #deleteQuery": "selectAndDeleteQueryFromTable",
      "click #explQuery": "selectAndExplainQueryFromTable",
      "keyup #arangoBindParamTable input": "updateBindParams",
      "change #arangoBindParamTable input": "updateBindParams",
      "click #arangoMyQueriesTable tr" : "showQueryPreview",
      "dblclick #arangoMyQueriesTable tr" : "selectQueryFromTable",
      "click #arangoMyQueriesTable #copyQuery" : "selectQueryFromTable",
      'click #closeQueryModal': 'closeExportDialog',
      'click #confirmQueryImport': 'importCustomQueries',
      'click #switchTypes': 'toggleBindParams',
      "click #arangoMyQueriesTable #runQuery" : "selectAndRunQueryFromTable"
    },

    clearQuery: function() {
      this.aqlEditor.setValue('');
    },

    toggleBindParams: function() {

      if (this.allowParamToggle) {
        $('#bindParamEditor').toggle();
        $('#bindParamAceEditor').toggle();

        if ($('#switchTypes').text() === 'JSON') {
          $('#switchTypes').text('Table');
          this.updateQueryTable();
          this.bindParamAceEditor.setValue(JSON.stringify(this.bindParamTableObj, null, "\t"));
          this.deselect(this.bindParamAceEditor);
        }
        else {
          $('#switchTypes').text('JSON');
          this.renderBindParamTable();
        }
      }
      else {
        arangoHelper.arangoError("Bind parameter", "Could not parse bind parameter");
      }
      this.resize();

    },

    openExportDialog: function() {
      $('#queryImportDialog').modal('show'); 
    },

    closeExportDialo: function() {
      $('#queryImportDialog').modal('hide'); 
    },

    initQueryImport: function () {
      var self = this;
      self.allowUpload = false;
      $('#importQueries').change(function(e) {
        self.files = e.target.files || e.dataTransfer.files;
        self.file = self.files[0];

        self.allowUpload = true;
        $('#confirmQueryImport').removeClass('disabled');
      });
    },

    importCustomQueries: function () {
      var self = this;
      if (this.allowUpload === true) {

        var callback = function() {
          this.collection.fetch({
            success: function() {
              self.updateLocalQueries();
              self.updateQueryTable();
              self.resize();
              self.allowUpload = false;
              $('#confirmQueryImport').addClass('disabled');
              $('#queryImportDialog').modal('hide'); 
            },
            error: function(data) {
              arangoHelper.arangoError("Custom Queries", data.responseText);
            }
          });
        }.bind(this);

        self.collection.saveImportQueries(self.file, callback.bind(this));
      }
    },

    removeResults: function() {
      $('.outputEditorWrapper').hide('fast', function() {
        $('.outputEditorWrapper').remove();
      });
      $('#removeResults').hide();
    },

    getCustomQueryParameterByName: function (qName) {
      return this.collection.findWhere({name: qName}).get("parameter");
    },

    getCustomQueryValueByName: function (qName) {
      var obj;

      if (qName) {
        obj = this.collection.findWhere({name: qName});
      }
      if (obj) {
        obj = obj.get("value");
      }
      else {
        _.each(this.queries, function(query) {
          if (query.name === qName) {
            obj = query.value;
          }
        });
      }
      return obj;
    },

    openImportDialog: function() {
      $('#queryImportDialog').modal('show'); 
    },

    closeImportDialog: function() {
      $('#queryImportDialog').modal('hide'); 
    },

    exportCustomQueries: function () {
      var name;

      $.ajax("whoAmI?_=" + Date.now()).success(function(data) {
        name = data.user;

        if (name === null || name === false) {
          name = "root";
        }
        window.open("query/download/" + encodeURIComponent(name));
      });
    },

    toggleQueries: function(e) {

      if (e) {
        if (e.currentTarget.id === "toggleQueries1") {
          this.updateQueryTable();
          $('#bindParamAceEditor').hide();
          $('#bindParamEditor').show();
          $('#switchTypes').text('JSON');
        }
      }

      var divs = [
        "aqlEditor", "queryTable", "previewWrapper", "querySpotlight",
        "bindParamEditor", "toggleQueries1", "toggleQueries2",
        "saveCurrentQuery", "querySize", "executeQuery", "switchTypes", 
        "explainQuery", "clearQuery", "importQuery", "exportQuery"
      ];
      _.each(divs, function(div) {
        $("#" + div).toggle();
      });
      this.resize();
    },

    showQueryPreview: function(e) {
      $('#arangoMyQueriesTable tr').removeClass('selected');
      $(e.currentTarget).addClass('selected');

      var name = this.getQueryNameFromTable(e);
      this.queryPreview.setValue(this.getCustomQueryValueByName(name));
      this.deselect(this.queryPreview);
    },

    getQueryNameFromTable: function(e) {
      var name; 
      if ($(e.currentTarget).is('tr')) {
        name = $(e.currentTarget).children().first().text();
      }
      else if ($(e.currentTarget).is('span')) {
        name = $(e.currentTarget).parent().parent().prev().prev().text();
      }
      return name;
    },

    deleteQueryModal: function(name){
      var buttons = [], tableContent = [];
      tableContent.push(
        window.modalView.createReadOnlyEntry(
          undefined,
          name,
          'Do you want to delete the query?',
          undefined,
          undefined,
          false,
          undefined
        )
      );
      buttons.push(
        window.modalView.createDeleteButton('Delete', this.deleteAQL.bind(this, name))
      );
      window.modalView.show(
        'modalTable.ejs', 'Delete Query', buttons, tableContent
      );
    },

    selectAndDeleteQueryFromTable: function(e) {
      var name = this.getQueryNameFromTable(e);
      this.deleteQueryModal(name);
    },

    selectAndExplainQueryFromTable: function(e) {
      this.selectQueryFromTable(e, false);
      this.explainQuery();
    },

    selectAndRunQueryFromTable: function(e) {
      this.selectQueryFromTable(e, false);
      this.executeQuery();
    },

    selectQueryFromTable: function(e, toggle) {
      var name = this.getQueryNameFromTable(e);

      if (toggle === undefined) {
        this.toggleQueries();
      }

      this.aqlEditor.setValue(this.getCustomQueryValueByName(name));
      this.fillBindParamTable(this.getCustomQueryParameterByName(name));
      this.updateBindParams();
      this.deselect(this.aqlEditor);
    },

    deleteAQL: function (name) {
      var callbackRemove = function(error) {
        if (error) {
          arangoHelper.arangoError("Query", "Could not delete query.");
        }
        else {
          this.updateLocalQueries();
          this.updateQueryTable();
          this.resize();
          window.modalView.hide();
        }
      }.bind(this);

      var toDelete = this.collection.findWhere({name: name});

      this.collection.remove(toDelete);
      this.collection.saveCollectionQueries(callbackRemove);
    },

    switchAce: function(e) {
      var count = $(e.currentTarget).attr('counter');

      if ($(e.currentTarget).text() === 'Result') {
        $(e.currentTarget).text('AQL');
      }
      else {
        $(e.currentTarget).text('Result');
      }
      $('#outputEditor' + count).toggle();
      $('#sentQueryEditor' + count).toggle();
    },

    downloadQueryResult: function(e) {
      var count = $(e.currentTarget).attr('counter'),
      editor = ace.edit("sentQueryEditor" + count),
      query = editor.getValue();

      if (query !== '' || query !== undefined || query !== null) {
        window.open("query/result/download/" + encodeURIComponent(btoa(JSON.stringify({ query: query }))));
      }
      else {
        arangoHelper.arangoError("Query error", "could not query result.");
      }
    },

    timer: {

      begin: 0,
      end: 0,

      start: function() {
        this.begin = new Date().getTime();
      },

      stop: function() {
        this.end = new Date().getTime();
      },

      reset: function() {
        this.begin = 0;
        this.end = 0;
      },

      getTimeAndReset: function() {
        this.stop();
        var result =  this.end - this.begin;
        this.reset();

        return result;
      }
    },

    explainQuery: function() {

      if (this.verifyQueryAndParams()) {
        return;
      }

      this.$(this.outputDiv).prepend(this.outputTemplate.render({
        counter: this.outputCounter,
        type: "Explain"
      }));

      var counter = this.outputCounter,
      outputEditor = ace.edit("outputEditor" + counter),
      sentQueryEditor = ace.edit("sentQueryEditor" + counter);
      sentQueryEditor.getSession().setMode("ace/mode/aql");
      outputEditor.getSession().setMode("ace/mode/json");
      outputEditor.setOption("vScrollBarAlwaysVisible", true);
      sentQueryEditor.setOption("vScrollBarAlwaysVisible", true);
      outputEditor.setReadOnly(true);
      sentQueryEditor.setReadOnly(true);

      this.fillExplain(outputEditor, sentQueryEditor, counter);
      this.outputCounter++;
    },

    fillExplain: function(outputEditor, sentQueryEditor, counter) {
      sentQueryEditor.setValue(this.aqlEditor.getValue());

      var self = this,
      queryData = this.readQueryData();
      $('#outputEditorWrapper' + counter + ' .queryExecutionTime').text('');
      this.execPending = false;

      if (queryData) {
        window.progressView.show(
          "Explain is operating..."
        );

        $.ajax({
          type: "POST",
          url: "/_admin/aardvark/query/explain/",
          data: queryData,
          contentType: "application/json",
          processData: false,
          success: function (data) {
            if (data.msg.includes('errorMessage')) {
              self.removeOutputEditor(counter);
              arangoHelper.arangoError("Explain error", data.msg);
              window.progressView.hide();
            }
            else {
              outputEditor.setValue(data.msg);
              self.deselect(outputEditor);
              $.noty.clearQueue();
              $.noty.closeAll();
              self.handleResult(counter);
            }
          },
          error: function (data) {
            try {
              var temp = JSON.parse(data.responseText);
              arangoHelper.arangoError("Explain error", temp.errorMessage);
            }
            catch (e) {
              arangoHelper.arangoError("Explain error", "ERROR");
            }
            window.progressView.hide();
            self.handleResult(counter);
            this.removeOutputEditor(counter);
          }
        });
      }
    },

    removeOutputEditor: function(counter) {
      $('#outputEditorWrapper' + counter).hide();
      $('#outputEditorWrapper' + counter).remove();
      if ($('.outputEditorWrapper').length === 0) {
        $('#removeResults').hide(); 
      }
    },

    getCachedQueryAfterRender: function() {
      //get cached query if available
      var queryObject = this.getCachedQuery(),
      self = this;

      if (queryObject !== null && queryObject !== undefined && queryObject !== "") {
        if (queryObject.parameter !== '' || queryObject !== undefined) {
          try {
            self.bindParamTableObj = JSON.parse(queryObject.parameter);
          }
          catch (ignore) {
          }
        }
        this.aqlEditor.setValue(queryObject.query);
      }
    },

    getCachedQuery: function() {
      if (Storage !== "undefined") {
        var cache = localStorage.getItem("cachedQuery");
        if (cache !== undefined) {
          var query = JSON.parse(cache);
          this.currentQuery = query;
          try {
            this.bindParamTableObj = JSON.parse(query.parameter);
          }
          catch (ignore) {
          }
          return query;
        }
      }
    },

    setCachedQuery: function(query, vars) {
      if (Storage !== "undefined") {
        var myObject = {
          query: query,
          parameter: vars
        };
        this.currentQuery = myObject;
        localStorage.setItem("cachedQuery", JSON.stringify(myObject));
      }
    },

    closeResult: function(e) {
      var target = $("#" + $(e.currentTarget).attr('element')).parent();
      $(target).hide('fast', function() {
        $(target).remove();
        if ($('.outputEditorWrapper').length === 0) {
          $('#removeResults').hide();
        }
      });
    },

    fillSelectBoxes: function() {
      // fill select box with # of results
      var querySize = 1000,
      sizeBox = $('#querySize');
      sizeBox.empty();

      [ 100, 250, 500, 1000, 2500, 5000, 10000, "all" ].forEach(function (value) {
        sizeBox.append('<option value="' + _.escape(value) + '"' +
          (querySize === value ? ' selected' : '') +
          '>' + _.escape(value) + ' results</option>');
      });
    },

    render: function() {
      this.$el.html(this.template.render({}));

      this.afterRender();
      this.initDone = true;
      this.renderBindParamTable(true);
    },

    afterRender: function() {
      var self = this;
      this.initAce();
      this.initTables();
      this.getCachedQueryAfterRender();
      this.fillSelectBoxes();
      this.makeResizeable();
      this.initQueryImport();

      //set height of editor wrapper
      $('.inputEditorWrapper').height($(window).height() / 10 * 5 + 25);
      window.setTimeout(function() {
        self.resize();
      }, 10);
      self.deselect(self.aqlEditor);
    },

    showSpotlight: function() {

      var callback = function(string) {
        this.aqlEditor.insert(string);
        $('#aqlEditor .ace_text-input').focus();
      }.bind(this);

      var cancelCallback = function() {
        $('#aqlEditor .ace_text-input').focus();
      };

      window.spotlightView.show(callback, cancelCallback);
    },

    resize: function() {
      this.resizeFunction();
    },

    resizeFunction: function() {
      if ($('#toggleQueries1').is(':visible')) {
        this.aqlEditor.resize();
        //fix bind table resizing issues
        $('#arangoBindParamTable thead').css('width', $('#bindParamEditor').width());
        $('#arangoBindParamTable thead th').css('width', $('#bindParamEditor').width() / 2);
        $('#arangoBindParamTable tr').css('width', $('#bindParamEditor').width());
        $('#arangoBindParamTable tbody').css('height', $('#aqlEditor').height() - 18);
        $('#arangoBindParamTable tbody').css('width', $('#bindParamEditor').width());
        $('#arangoBindParamTable tbody tr').css('width', $('#bindParamEditor').width());
        $('#arangoBindParamTable tbody td').css('width', $('#bindParamEditor').width() / 2);
      }
      else {
        this.queryPreview.resize();
        //fix my queries preview table resizing issues TODO
        $('#arangoMyQueriesTable thead').css('width', $('#queryTable').width());
        $('#arangoMyQueriesTable thead th').css('width', $('#queryTable').width() / 3);
        $('#arangoMyQueriesTable tr').css('width', $('#queryTable').width());
        $('#arangoMyQueriesTable tbody').css('height', $('#queryTable').height() - 18);
        $('#arangoMyQueriesTable tbody').css('width', $('#queryTable').width());
        $('#arangoMyQueriesTable tbody tr').css('width', $('#queryTable').width());
        $('#arangoMyQueriesTable tbody td').css('width', $('#queryTable').width() / 3);
        $('#arangoMyQueriesTable tbody td .truncate').css('width', $('#queryTable').width() / 3);
      }
    },

    makeResizeable: function() {
      var self = this;

      $(".aqlEditorWrapper").resizable({
        resize: function() {
          self.resizeFunction();
        },
        handles: "e"
      });

      $(".inputEditorWrapper").resizable({
        resize: function() {
          self.resizeFunction();
        },
        handles: "s"
      });

      //one manual start
      this.resizeFunction();
    },

    initTables: function() {
       this.$(this.bindParamId).html(this.table.render({content: this.bindParamTableDesc}));
       this.$(this.myQueriesId).html(this.table.render({content: this.myQueriesTableDesc}));
    },

    checkType: function(val) {
      var type = "stringtype";

      try {
        val = JSON.parse(val);
        if (val instanceof Array) {
          type = 'arraytype';
        }
        else {
          type = typeof val + 'type';
        }
      }
      catch(ignore) {
      }

      return type;
    },

    updateBindParams: function(e) {

      var id, self = this;

      if (e) {
        id = $(e.currentTarget).attr("name");
        //this.bindParamTableObj[id] = $(e.currentTarget).val();
        this.bindParamTableObj[id] = arangoHelper.parseInput(e.currentTarget);

        var types = [
          "arraytype", "objecttype", "booleantype", "numbertype", "stringtype"
        ];
        _.each(types, function(type) {
          $(e.currentTarget).removeClass(type);
        });
        $(e.currentTarget).addClass(self.checkType($(e.currentTarget).val()));
      }
      else {
        _.each($('#arangoBindParamTable input'), function(element) {
          id = $(element).attr("name");
          self.bindParamTableObj[id] = arangoHelper.parseInput(element);
        });
      }
      this.setCachedQuery(this.aqlEditor.getValue(), JSON.stringify(this.bindParamTableObj));
    },

    checkForNewBindParams: function() {
      var self = this,
      words = (this.aqlEditor.getValue()).split(" "),
      words1 = [],
      pos = 0;

      _.each(words, function(word) {
        word = word.split("\n");
        _.each(word, function(x) {
          words1.push(x);
        });
      });

      _.each(words, function(word) {
        word = word.split(",");
        _.each(word, function(x) {
          words1.push(x);
        });
      });

      _.each(words1, function(word) {
        // remove newlines and whitespaces
        words[pos] = word.replace(/(\r\n|\n|\r)/gm,"");
        pos++;
      });

      var newObject = {};
      _.each(words1, function(word) {
        //found a valid bind param expression
        var match = word.match(self.bindParamRegExp);
        if (match) {
          //if property is not available
          word = match[1];
          newObject[word] = '';
        }
      });

      Object.keys(newObject).forEach(function(keyNew) {
        Object.keys(self.bindParamTableObj).forEach(function(keyOld) {
          if (keyNew === keyOld) {
            newObject[keyNew] = self.bindParamTableObj[keyOld];
          }
        });
      });
      self.bindParamTableObj = newObject;
    },

    renderBindParamTable: function(init) {
      $('#arangoBindParamTable tbody').html('');

      if (init) {
        this.getCachedQuery();
      }

      var counter = 0;

      _.each(this.bindParamTableObj, function(val, key) {
        $('#arangoBindParamTable tbody').append(
          "<tr>" + 
            "<td>" + key + "</td>" + 
            '<td><input name=' + key + ' type="text"></input></td>' + 
          "</tr>"
        );
        counter ++;
        _.each($('#arangoBindParamTable input'), function(element) {
          if ($(element).attr('name') === key) {
            if (val instanceof Array) {
              $(element).val(JSON.stringify(val)).addClass('arraytype');
            }
            else if (typeof val === 'object') {
              $(element).val(JSON.stringify(val)).addClass(typeof val + 'type');
            }
            else {
              $(element).val(val).addClass(typeof val + 'type');
            }
          }
        });
      });
      if (counter === 0) {
        $('#arangoBindParamTable tbody').append(
          '<tr class="noBgColor">' + 
            "<td>No bind parameters defined.</td>" + 
            '<td></td>' + 
          "</tr>"
        );
      }
    },

    fillBindParamTable: function(object) {
      _.each(object, function(val, key) {
        _.each($('#arangoBindParamTable input'), function(element) {
          if ($(element).attr('name') === key) {
            $(element).val(val);
          }
        });
      });
    },

    initAce: function() {

      var self = this;

      //init aql editor
      this.aqlEditor = ace.edit("aqlEditor");
      this.aqlEditor.getSession().setMode("ace/mode/aql");
      this.aqlEditor.setFontSize("13px");

      this.bindParamAceEditor = ace.edit("bindParamAceEditor");
      this.bindParamAceEditor.getSession().setMode("ace/mode/json");
      this.bindParamAceEditor.setFontSize("13px");

      this.bindParamAceEditor.getSession().on('change', function() {
        try {
          self.bindParamTableObj = JSON.parse(self.bindParamAceEditor.getValue());
          self.allowParamToggle = true;
        }
        catch (e) {
          self.allowParamToggle = false;
        }
      });

      this.aqlEditor.getSession().on('change', function() {
        self.checkForNewBindParams();
        self.renderBindParamTable();
        if (self.initDone) {
          self.setCachedQuery(self.aqlEditor.getValue(), JSON.stringify(self.bindParamTableObj));
        }
        self.bindParamAceEditor.setValue(JSON.stringify(self.bindParamTableObj, null, "\t"));
        $('#aqlEditor .ace_text-input').focus();

        self.resize();
      });

      this.aqlEditor.commands.addCommand({
        name: "togglecomment",
        bindKey: {win: "Ctrl-Shift-C", linux: "Ctrl-Shift-C", mac: "Command-Shift-C"},
        exec: function (editor) {
          editor.toggleCommentLines();
        },
        multiSelectAction: "forEach"
      });

      this.aqlEditor.commands.addCommand({
        name: "executeQuery",
        bindKey: {win: "Ctrl-Return", mac: "Command-Return", linux: "Ctrl-Return"},
        exec: function() {
          self.executeQuery();
        }
      });

      this.aqlEditor.commands.addCommand({
        name: "saveQuery",
        bindKey: {win: "Ctrl-Shift-S", mac: "Command-Shift-S", linux: "Ctrl-Shift-S"},
        exec: function() {
          self.addAQL();
        }
      });

      this.aqlEditor.commands.addCommand({
        name: "explainQuery",
        bindKey: {win: "Ctrl-Shift-Return", mac: "Command-Shift-Return", linux: "Ctrl-Shift-Return"},
        exec: function() {
          self.explainQuery();
        }
      });

      this.aqlEditor.commands.addCommand({
        name: "showSpotlight",
        bindKey: {win: "Ctrl-Space", mac: "Ctrl-Space", linux: "Ctrl-Space"},
        exec: function() {

          self.showSpotlight();
        }
      });

      this.queryPreview = ace.edit("queryPreview");
      this.queryPreview.getSession().setMode("ace/mode/aql");
      this.queryPreview.setReadOnly(true);
      this.queryPreview.setFontSize("13px");

      //auto focus this editor
      $('#aqlEditor .ace_text-input').focus();
    },

    updateQueryTable: function () {
      var self = this;
      this.updateLocalQueries();

      this.myQueriesTableDesc.rows = this.customQueries;
      _.each(this.myQueriesTableDesc.rows, function(k) {
        k.secondRow = '<div class="truncate">' +
          JSON.stringify(self.collection.findWhere({name: k.name}).get('value')) +
        '</div>';

        k.thirdRow = '<span class="spanWrapper">' + 
          '<span id="copyQuery" title="Copy query"><i class="fa fa-copy"></i></span>' +
          '<span id="explQuery" title="Explain query"><i class="fa fa-comments"></i></i></span>' +
          '<span id="runQuery" title="Run query"><i class="fa fa-play-circle-o"></i></i></span>' +
          '<span id="deleteQuery" title="Delete query"><i class="fa fa-minus-circle"></i></span>' +
          '</span>';
        if (k.hasOwnProperty('parameter')) {
          delete k.parameter;
        }
        delete k.value;
      });

      function compare(a,b) {
        if (a.name < b.name) {
          return -1;
        }
        else if (a.name > b.name) {
          return 1;
        }
        else {
          return 0;
        }
      }

      this.myQueriesTableDesc.rows.sort(compare);

      _.each(this.queries, function(val) {
        if (val.hasOwnProperty('parameter')) {
          delete val.parameter;
        }
        self.myQueriesTableDesc.rows.push({
          name: val.name,
          secondRow: '<div class="truncate">' + val.value + '</div>',
          thirdRow: '<span class="spanWrapper">' + 
            '<span id="copyQuery" title="Copy query"><i class="fa fa-copy"></i></span></span>' 
        });
      });

      // escape all columns but the third (which contains HTML)
      this.myQueriesTableDesc.unescaped = [ false, true, true ];


      this.$(this.myQueriesId).html(this.table.render({content: this.myQueriesTableDesc}));
    },

    listenKey: function (e) {
      if (e.keyCode === 13) {
        this.saveAQL(e);
      }
      this.checkSaveName();
    },

    addAQL: function () {
      //update queries first, before showing
      this.refreshAQL(true);

      //render options
      this.createCustomQueryModal();
      setTimeout(function () {
        $('#new-query-name').focus();
      }, 500);
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
      window.modalView.show('modalTable.ejs', 'Save Query', buttons, tableContent, undefined, undefined,
        {'keyup #new-query-name' : this.listenKey.bind(this)});
    },

    checkSaveName: function() {
      var saveName = $('#new-query-name').val();
      if ( saveName === "Insert Query"){
        $('#new-query-name').val('');
        return;
      }

      //check for invalid query names, if present change the box-shadow to red
      // and disable the save functionality
      var found = this.customQueries.some(function(query){
        return query.name === saveName;
      });
      if (found) {
        $('#modalButton1').removeClass('button-success');
        $('#modalButton1').addClass('button-warning');
        $('#modalButton1').text('Update');
      } 
      else {
        $('#modalButton1').removeClass('button-warning');
        $('#modalButton1').addClass('button-success');
        $('#modalButton1').text('Save');
      }
    },

    saveAQL: function (e) {
      e.stopPropagation();

      //update queries first, before writing
      this.refreshAQL();

      var saveName = $('#new-query-name').val(),
      bindVars = this.bindParamTableObj;

      if ($('#new-query-name').hasClass('invalid-input')) {
        return;
      }

      //Heiko: Form-Validator - illegal query name
      if (saveName.trim() === '') {
        return;
      }

      var content = this.aqlEditor.getValue(),
      //check for already existing entry
      quit = false;
      _.each(this.customQueries, function (v) {
        if (v.name === saveName) {
          v.value = content;
          quit = true;
          return;
        }
      });

      if (quit === true) {
        //Heiko: Form-Validator - name already taken
        // Update model and save
        this.collection.findWhere({name: saveName}).set("value", content);
      }
      else {
        if (bindVars === '' || bindVars === undefined) {
          bindVars = '{}';
        }

        if (typeof bindVars === 'string') {
          try {
            bindVars = JSON.parse(bindVars);
          }
          catch (err) {
            arangoHelper.arangoError("Query", "Could not parse bind parameter");
          }
        }
        this.collection.add({
          name: saveName,
          parameter: bindVars, 
          value: content
        });
      }

      var callback = function(error) {
        if (error) {
          arangoHelper.arangoError("Query", "Could not save query");
        }
        else {
          var self = this;
          this.collection.fetch({
            success: function() {
              self.updateLocalQueries();
            }
          });
        }
      }.bind(this);
      this.collection.saveCollectionQueries(callback);
      window.modalView.hide();
    },

    verifyQueryAndParams: function() {
      var quit = false;

      if (this.aqlEditor.getValue().length === 0) {
        arangoHelper.arangoError("Query", "Your query is empty");
        quit = true;
      }

      var keys = [];
      _.each(this.bindParamTableObj, function(val, key) {
        if (val === '') {
          quit = true;
          keys.push(key);
        }
      });
      if (keys.length > 0) {
        arangoHelper.arangoError("Bind Parameter", JSON.stringify(keys) + " not defined.");
      }

      return quit;
    },

    executeQuery: function () {

      if (this.verifyQueryAndParams()) {
        return;
      }

      this.$(this.outputDiv).prepend(this.outputTemplate.render({
        counter: this.outputCounter,
        type: "Query"
      }));

      $('#outputEditorWrapper' + this.outputCounter).hide();
      $('#outputEditorWrapper' + this.outputCounter).show('fast');

      var counter = this.outputCounter,
      outputEditor = ace.edit("outputEditor" + counter),
      sentQueryEditor = ace.edit("sentQueryEditor" + counter);
      sentQueryEditor.getSession().setMode("ace/mode/aql");
      outputEditor.getSession().setMode("ace/mode/json");
      outputEditor.setReadOnly(true);
      outputEditor.setOption("vScrollBarAlwaysVisible", true);
      sentQueryEditor.setOption("vScrollBarAlwaysVisible", true);
      outputEditor.setFontSize("13px");
      sentQueryEditor.setFontSize("13px");
      sentQueryEditor.setReadOnly(true);

      this.fillResult(outputEditor, sentQueryEditor, counter);
      this.outputCounter++;
    },

    readQueryData: function() {
      var selectedText = this.aqlEditor.session.getTextRange(this.aqlEditor.getSelectionRange());
      var sizeBox = $('#querySize');
      var data = {
        query: selectedText || this.aqlEditor.getValue(),
        id: "currentFrontendQuery"
      };

      if (sizeBox.val() !== 'all') {
        data.batchSize = parseInt(sizeBox.val(), 10);
      }

      var parsedBindVars = {}, tmpVar;
      if (Object.keys(this.bindParamTableObj).length > 0) {
        _.each(this.bindParamTableObj, function(value, key) {
          try {
            tmpVar = JSON.parse(value);
          }
          catch (e) {
            tmpVar = value;
          }
          parsedBindVars[key] = tmpVar;
        });
        data.bindVars = parsedBindVars;
      }
      return JSON.stringify(data);
    },

    fillResult: function(outputEditor, sentQueryEditor, counter) {
      var self = this;

      var queryData = this.readQueryData();
      if (queryData) {
        sentQueryEditor.setValue(self.aqlEditor.getValue());

        $.ajax({
          type: "POST",
          url: "/_api/cursor",
          headers: {
            'x-arango-async': 'store' 
          },
          data: queryData,
          contentType: "application/json",
          processData: false,
          success: function (data, textStatus, xhr) {
            if (xhr.getResponseHeader('x-arango-async-id')) {
              self.queryCallbackFunction(xhr.getResponseHeader('x-arango-async-id'), outputEditor, counter);
            }
            $.noty.clearQueue();
            $.noty.closeAll();
            self.handleResult(counter);
          },
          error: function (data) {
            try {
              var temp = JSON.parse(data.responseText);
              arangoHelper.arangoError('[' + temp.errorNum + ']', temp.errorMessage);
            }
            catch (e) {
              arangoHelper.arangoError("Query error", "ERROR");
            }
            self.handleResult(counter);
          }
        });
      }
    },

    handleResult: function(counter) {
      window.progressView.hide();
      $('#removeResults').show();

      //TODO animate not sure
      //$('html,body').animate({
      //  scrollTop: $('#outputEditorWrapper' + counter).offset().top - 120
      //}, 500);
    },

    setEditorAutoHeight: function (editor) {
      editor.setOptions({
        maxLines: Infinity
      }); 
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

    queryCallbackFunction: function(queryID, outputEditor, counter) {

      var self = this;

      var cancelRunningQuery = function() {

        $.ajax({
          url: '/_api/job/'+ encodeURIComponent(queryID) + "/cancel",
          type: 'PUT',
          success: function() {
            window.clearTimeout(self.checkQueryTimer);
            arangoHelper.arangoNotification("Query", "Query canceled.");
            window.progressView.hide();
            //TODO REMOVE OUTPUT BOX
          }
        });
      };

      window.progressView.show(
        "Query is operating...",
        cancelRunningQuery,
        "Cancel Query"
      );

      self.timer.start();
      this.execPending = false;

      var warningsFunc = function(data) {
        var warnings = "";
        if (data.extra && data.extra.warnings && data.extra.warnings.length > 0) {
          warnings += "Warnings:" + "\r\n\r\n";
          data.extra.warnings.forEach(function(w) {
            warnings += "[" + w.code + "], '" + w.message + "'\r\n";
          });
        }
        if (warnings !== "") {
          warnings += "\r\n" + "Result:" + "\r\n\r\n";
        }
        outputEditor.setValue(warnings + JSON.stringify(data.result, undefined, 2));
      };

      var fetchQueryResult = function(data) {
        warningsFunc(data);
        window.progressView.hide();

        var time = self.timer.getTimeAndReset()/1000 + " s";
        $('#outputEditorWrapper' + counter + ' .queryExecutionTime2').text(time);

        self.setEditorAutoHeight(outputEditor);
        self.deselect(outputEditor);
      };

      //check if async query is finished
      var checkQueryStatus = function() {
        $.ajax({
          type: "PUT",
          url: "/_api/job/" + encodeURIComponent(queryID),
          contentType: "application/json",
          processData: false,
          success: function (data, textStatus, xhr) {

            //query finished, now fetch results
            if (xhr.status === 201) {
              fetchQueryResult(data);
            }
            //query not ready yet, retry
            else if (xhr.status === 204) {
              self.checkQueryTimer = window.setTimeout(function() {
                checkQueryStatus(); 
              }, 500);
            }
          },
          error: function (resp) {
            try {
              var error = JSON.parse(resp.responseText);
              if (error.errorMessage) {
                if (error.errorMessage.match(/\d+:\d+/g) !== null) {
                  self.markPositionError(
                    error.errorMessage.match(/'.*'/g)[0],
                    error.errorMessage.match(/\d+:\d+/g)[0]
                  );
                }
                else {
                  self.markPositionError(
                    error.errorMessage.match(/\(\w+\)/g)[0]
                  );
                }
                arangoHelper.arangoError("Query", error.errorMessage);
                self.removeOutputEditor(counter);
              }
            }
            catch (e) {
              arangoHelper.arangoError("Query", "Something went wrong.");
              self.removeOutputEditor(counter);
            }

            window.progressView.hide();
          }
        });
      };
      checkQueryStatus();
    },

    markPositionError: function(text, pos) {
      var row;

      if (pos) {
        row = pos.split(":")[0];
        text = text.substr(1, text.length - 2);
      }

      var found = this.aqlEditor.find(text);

      if (!found && pos) {
        this.aqlEditor.selection.moveCursorToPosition({row: row, column: 0});
        this.aqlEditor.selection.selectLine(); 
      }
      window.setTimeout(function() {
        $('.ace_start').first().css('background', 'rgba(255, 129, 129, 0.7)');
      }, 100);
    },

    refreshAQL: function() {
      var self = this;

      var callback = function(error) {
        if (error) {
          arangoHelper.arangoError('Query', 'Could not reload Queries');
        }
        else {
          self.updateLocalQueries();
          self.updateQueryTable();
        }
      }.bind(self);

      var originCallback = function() {
        self.getSystemQueries(callback);
      }.bind(self);

      this.getAQL(originCallback);
    },

    getSystemQueries: function (callback) {
      var self = this;

      $.ajax({
        type: "GET",
        cache: false,
        url: "js/arango/aqltemplates.json",
        contentType: "application/json",
        processData: false,
        success: function (data) {
          if (callback) {
            callback(false);
          }
          self.queries = data;
        },
        error: function () {
          if (callback) {
            callback(true);
          }
          arangoHelper.arangoNotification("Query", "Error while loading system templates");
        }
      });
    },

    updateLocalQueries: function () {
      var self = this;
      this.customQueries = [];

      this.collection.each(function(model) {
        self.customQueries.push({
          name: model.get("name"),
          value: model.get("value"),
          parameter: model.get("parameter")
        });
      });
    },

    getAQL: function (originCallback) {
      var self = this;

      this.collection.fetch({
        success: function() {
          //old storage method
          var item = localStorage.getItem("customQueries");
          if (item) {
            var queries = JSON.parse(item);
            //save queries in user collections extra attribute
            _.each(queries, function(oldQuery) {
              self.collection.add({
                value: oldQuery.value,
                name: oldQuery.name
              });
            });

            var callback = function(error) {
              if (error) {
                arangoHelper.arangoError(
                  "Custom Queries",
                  "Could not import old local storage queries"
                );
              }
              else {
                localStorage.removeItem("customQueries");
              }
            }.bind(self);
            self.collection.saveCollectionQueries(callback);
          }//TODO
          self.updateLocalQueries();

          if (originCallback) {
            originCallback();
          }
        }
      });
    }
  });
}());

