/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, EJS, $, setTimeout, localStorage, ace, Storage, window, _, console, btoa*/
/*global _, arangoHelper, templateEngine, jQuery, Joi, d3*/

(function () {
  "use strict";
  window.queryView2 = Backbone.View.extend({
    el: '#content',
    bindParamId: '#bindParamEditor',
    template: templateEngine.createTemplate("queryView2.ejs"),
    table: templateEngine.createTemplate("arangoTable.ejs"),

    outputDiv: '#outputEditors',
    outputTemplate: templateEngine.createTemplate("queryViewOutput.ejs"),
    outputCounter: 0,

    bindParamTableDesc: {
      id: "arangoBindParamTable",
      titles: ["Key", "Value"],
      rows: []
    },

    execPending: false,

    aqlEditor: null,
    paramEditor: null,

    initialize: function () {
      this.refreshAQL();
    },

    events: {
      "click #executeQuery": "executeQuery",
      "click #explainQuery": "explainQuery",
      "click #clearQuery": "clearQuery",
      'click .outputEditorWrapper #downloadQueryResult': 'downloadQueryResult',
      'click .outputEditorWrapper .switchAce': 'switchAce',
      "click .outputEditorWrapper .fa-close": "closeResult"
    },

    clearQuery: function() {
      this.aqlEditor.setValue('');
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
      if (this.aqlEditor.getValue().length === 0) {
        arangoHelper.arangoError("Query", "Your query is empty");
        return;
      }

      this.$(this.outputDiv).append(this.outputTemplate.render({
        counter: this.outputCounter,
        type: "Explain"
      }));

      var counter = this.outputCounter,
      outputEditor = ace.edit("outputEditor" + counter);
      outputEditor.setReadOnly(true);

      this.fillExplain(outputEditor, counter);
      this.outputCounter++;
    },

    fillExplain: function(outputEditor, counter) {

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
            outputEditor.setValue(data.msg);
            window.progressView.hide();
            self.deselect(outputEditor);
            $.noty.clearQueue();
            $.noty.closeAll();
          },
          error: function (data) {
            window.progressView.hide();
            try {
              var temp = JSON.parse(data.responseText);
              arangoHelper.arangoError("Explain error", temp.errorNum);
            }
            catch (e) {
              arangoHelper.arangoError("Explain error", "ERROR");
            }
            window.progressView.hide();
          }
        });
      }
    },

    getCachedQueryAfterRender: function() {
      //get cached query if available
      var queryObject = this.getCachedQuery();
      if (queryObject !== null && queryObject !== undefined && queryObject !== "") {
        this.aqlEditor.setValue(queryObject.query);
        if (queryObject.parameter === '' || queryObject === undefined) {
          //TODO update bind param table
          //varsEditor.setValue('{}');
        }
        else {
          //TODO update bind param table
          //varsEditor.setValue(queryObject.parameter);
        }
      }
      var a = this.aqlEditor.getValue();
      if (a.length === 1) {
        a = "";
      }
      this.setCachedQuery(a);
    },

    getCachedQuery: function() {
      if (Storage !== "undefined") {
        var cache = localStorage.getItem("cachedQuery");
        if (cache !== undefined) {
          var query = JSON.parse(cache);
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
        localStorage.setItem("cachedQuery", JSON.stringify(myObject));
      }
    },

    closeResult: function(e) {
      var target = $("#" + $(e.currentTarget).attr('element')).parent();
      $(target).hide('fast', function() {
        $(target).remove();
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
    },

    makeResizeable: function() {
      var self = this;

      $(".aqlEditorWrapper").resizable({
        resize: function() {
          self.aqlEditor.resize();
        },
        handles: "e"
      });

      $(".inputEditorWrapper").resizable({
        resize: function() {
          self.aqlEditor.resize();
        },
        handles: "s"
      });
    },

    initBindParamTable: function() {
       this.$(this.bindParamId).html(this.table.render({content: this.bindParamTableDesc}));
    },

    initAce: function() {
      //init aql editor
      this.aqlEditor = ace.edit("aqlEditor");
      this.aqlEditor.getSession().setMode("ace/mode/aql");
      this.aqlEditor.setFontSize("13px");
      this.aqlEditor.commands.addCommand({
        name: "togglecomment",
        bindKey: {win: "Ctrl-Shift-C", linux: "Ctrl-Shift-C", mac: "Command-Shift-C"},
        exec: function (editor) {
          editor.toggleCommentLines();
        },
        multiSelectAction: "forEach"
      });

      //auto focus this editor
      $('#aqlEditor .ace_text-input').focus();
    },

    afterRender: function() {
      this.makeResizeable();
      this.initAce();
      this.initBindParamTable();
      this.getCachedQueryAfterRender();
      this.fillSelectBoxes();

      //set height of editor wrapper
      $('.inputEditorWrapper').height($(window).height() / 10 * 3);
    },

    saveAQL: function (e) {
      e.stopPropagation();

      //update queries first, before writing
      this.refreshAQL();

      var varsEditor = ace.edit("varsEditor"),
      saveName = $('#new-query-name').val(),
      bindVars = varsEditor.getValue();

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
      $.each(this.customQueries, function (k, v) {
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
            console.log("could not parse bind parameter");
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
              self.renderSelectboxes();
              $('#querySelect').val(saveName);
            }
          });
        }
      }.bind(this);
      this.collection.saveCollectionQueries(callback);
      window.modalView.hide();
    },

    executeQuery: function () {
      if (this.aqlEditor.getValue().length === 0) {
        arangoHelper.arangoError("Query", "Your query is empty");
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
      outputEditor.setReadOnly(true);
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

      //parse vars
      //var bindVars = varsEditor.getValue();
      //TODO bind vars table include

      var bindVars = "";
      if (bindVars.length > 0) {
        try {
          var params = JSON.parse(bindVars);
          if (Object.keys(params).length !== 0) {
            data.bindVars = params;
          }
        }
        catch (e) {
          arangoHelper.arangoError("Query error", "Could not parse bind parameters.");
          return false;
        }
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
          },
          error: function (data) {
            try {
              var temp = JSON.parse(data.responseText);
              outputEditor.setValue('[' + temp.errorNum + '] ' + temp.errorMessage);
            }
            catch (e) {
              outputEditor.setValue('ERROR');
              arangoHelper.arangoError("Query error", "ERROR");
            }
            window.progressView.hide();
          }
        });
      }
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
                arangoHelper.arangoError("Query", error.errorMessage);
                $('#outputEditorWrapper' + counter).hide();
              }
            }
            catch (e) {
              arangoHelper.arangoError("Query", "Something went wrong.");
            }

            window.progressView.hide();
          }
        });
      };
      checkQueryStatus();
    },

    refreshAQL: function() {
      var self = this;

      var callback = function(error) {
        if (error) {
          arangoHelper.arangoError('Query', 'Could not reload Queries');
        }
        else {
          self.updateLocalQueries();
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
      var self = this, result;

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

            var callback = function(error, data) {
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
          }
          self.updateLocalQueries();

          if (originCallback) {
            originCallback();
          }
        }
      });
    }
  });
}());

