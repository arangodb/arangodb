/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, $, setTimeout, localStorage, ace, Storage, window, _, console, btoa*/
/* global _, arangoHelper, templateEngine, Joi */

(function () {
  'use strict';
  window.QueryView = Backbone.View.extend({
    el: '#content',
    bindParamId: '#bindParamEditor',
    myQueriesId: '#queryTable',
    template: templateEngine.createTemplate('queryView.ejs'),
    table: templateEngine.createTemplate('arangoTable.ejs'),

    outputDiv: '#outputEditors',
    outputTemplate: templateEngine.createTemplate('queryViewOutput.ejs'),
    outputCounter: 0,

    allowUpload: false,
    renderComplete: false,

    customQueries: [],
    queries: [],

    state: {
      lastQuery: {
        query: undefined,
        bindParam: undefined
      }
    },

    graphs: [],

    settings: {
      aqlWidth: undefined
    },

    currentQuery: {},
    initDone: false,

    bindParamRegExp: /@(@?\w+\d*)/,
    bindParamTableObj: {},

    bindParamTableDesc: {
      id: 'arangoBindParamTable',
      titles: ['Key', 'Value'],
      rows: []
    },

    myQueriesTableDesc: {
      id: 'arangoMyQueriesTable',
      titles: ['Name', 'Actions'],
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
      'click #executeQuery': 'executeQuery',
      'click #explainQuery': 'explainQuery',
      'click #clearQuery': 'clearQuery',
      'click .outputEditorWrapper #downloadQueryResult': 'downloadQueryResult',
      'click .outputEditorWrapper .switchAce span': 'switchAce',
      'click .outputEditorWrapper .fa-close': 'closeResult',
      'click #toggleQueries1': 'toggleQueries',
      'click #toggleQueries2': 'toggleQueries',
      'click #createNewQuery': 'createAQL',
      'click #saveCurrentQuery': 'addAQL',
      'click #updateCurrentQuery': 'updateAQL',
      'click #exportQuery': 'exportCustomQueries',
      'click #importQuery': 'openImportDialog',
      'click #removeResults': 'removeResults',
      'click #querySpotlight': 'showSpotlight',
      'click #deleteQuery': 'selectAndDeleteQueryFromTable',
      'click #explQuery': 'selectAndExplainQueryFromTable',
      'keydown #arangoBindParamTable input': 'updateBindParams',
      'change #arangoBindParamTable input': 'updateBindParams',
      'click #arangoMyQueriesTable tbody tr': 'showQueryPreview',
      'dblclick #arangoMyQueriesTable tbody tr': 'selectQueryFromTable',
      'click #arangoMyQueriesTable #copyQuery': 'selectQueryFromTable',
      'click #closeQueryModal': 'closeExportDialog',
      'click #confirmQueryImport': 'importCustomQueries',
      'click #switchTypes': 'toggleBindParams',
      'click #arangoMyQueriesTable #runQuery': 'selectAndRunQueryFromTable'
    },

    clearQuery: function () {
      this.aqlEditor.setValue('', 1);
    },

    toggleBindParams: function () {
      if (this.allowParamToggle) {
        $('#bindParamEditor').toggle();
        $('#bindParamAceEditor').toggle();

        if ($('#switchTypes').text() === 'JSON') {
          $('#switchTypes').text('Table');
          this.updateQueryTable();
          this.bindParamAceEditor.setValue(JSON.stringify(this.bindParamTableObj, null, '\t'), 1);
          this.deselect(this.bindParamAceEditor);
        } else {
          $('#switchTypes').text('JSON');
          this.renderBindParamTable();
        }
      } else {
        arangoHelper.arangoError('Bind parameter', 'Could not parse bind parameter');
      }
      this.resize();
    },

    openExportDialog: function () {
      $('#queryImportDialog').modal('show');
    },

    closeExportDialog: function () {
      $('#queryImportDialog').modal('hide');
    },

    initQueryImport: function () {
      var self = this;
      self.allowUpload = false;
      $('#importQueries').change(function (e) {
        self.files = e.target.files || e.dataTransfer.files;
        self.file = self.files[0];

        self.allowUpload = true;
        $('#confirmQueryImport').removeClass('disabled');
      });
    },

    importCustomQueries: function () {
      var self = this;
      if (this.allowUpload === true) {
        var callback = function () {
          this.collection.fetch({
            success: function () {
              self.updateLocalQueries();
              self.updateQueryTable();
              self.resize();
              self.allowUpload = false;
              $('#confirmQueryImport').addClass('disabled');
              $('#queryImportDialog').modal('hide');
            },
            error: function (data) {
              arangoHelper.arangoError('Custom Queries', data.responseText);
            }
          });
        }.bind(this);

        self.collection.saveImportQueries(self.file, callback.bind(this));
      }
    },

    removeResults: function () {
      $('.outputEditorWrapper').hide('fast', function () {
        $('.outputEditorWrapper').remove();
      });
      $('#removeResults').hide();
    },

    getCustomQueryParameterByName: function (qName) {
      return this.collection.findWhere({name: qName}).get('parameter');
    },

    getCustomQueryValueByName: function (qName) {
      var obj;

      if (qName) {
        obj = this.collection.findWhere({name: qName});
      }
      if (obj) {
        obj = obj.get('value');
      } else {
        _.each(this.queries, function (query) {
          if (query.name === qName) {
            obj = query.value;
          }
        });
      }
      return obj;
    },

    openImportDialog: function () {
      $('#queryImportDialog').modal('show');
    },

    closeImportDialog: function () {
      $('#queryImportDialog').modal('hide');
    },

    exportCustomQueries: function () {
      var name;

      $.ajax('whoAmI?_=' + Date.now()).success(function (data) {
        name = data.user;

        if (name === null || name === false) {
          name = 'root';
        }
        var url = 'query/download/' + encodeURIComponent(name);
        arangoHelper.download(url);
      });
    },

    toggleQueries: function (e) {
      if (e) {
        if (e.currentTarget.id === 'toggleQueries1') {
          this.updateQueryTable();
          $('#bindParamAceEditor').hide();
          $('#bindParamEditor').show();
          $('#switchTypes').text('JSON');
          $('.aqlEditorWrapper').first().width($(window).width() * 0.33);
          this.queryPreview.setValue('No query selected.', 1);
          this.deselect(this.queryPreview);
        } else {
          $('#updateCurrentQuery').hide();
          if (this.settings.aqlWidth === undefined) {
            $('.aqlEditorWrapper').first().width($(window).width() * 0.33);
          } else {
            $('.aqlEditorWrapper').first().width(this.settings.aqlWidth);
          }

          if (localStorage.getItem('lastOpenQuery') !== 'undefined') {
            $('#updateCurrentQuery').show();
          }
        }
      } else {
        if (this.settings.aqlWidth === undefined) {
          $('.aqlEditorWrapper').first().width($(window).width() * 0.33);
        } else {
          $('.aqlEditorWrapper').first().width(this.settings.aqlWidth);
        }
      }
      this.resize();

      var divs = [
        'aqlEditor', 'queryTable', 'previewWrapper', 'querySpotlight',
        'bindParamEditor', 'toggleQueries1', 'toggleQueries2', 'createNewQuery',
        'saveCurrentQuery', 'querySize', 'executeQuery', 'switchTypes',
        'explainQuery', 'importQuery', 'exportQuery'
      ];
      _.each(divs, function (div) {
        $('#' + div).toggle();
      });
      this.resize();
    },

    showQueryPreview: function (e) {
      $('#arangoMyQueriesTable tr').removeClass('selected');
      $(e.currentTarget).addClass('selected');

      var name = this.getQueryNameFromTable(e);
      this.queryPreview.setValue(this.getCustomQueryValueByName(name), 1);
      this.deselect(this.queryPreview);
    },

    getQueryNameFromTable: function (e) {
      var name;
      if ($(e.currentTarget).is('tr')) {
        name = $(e.currentTarget).children().first().text();
      } else if ($(e.currentTarget).is('span')) {
        name = $(e.currentTarget).parent().parent().prev().text();
      }
      return name;
    },

    deleteQueryModal: function (name) {
      var buttons = [];
      var tableContent = [];
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

    selectAndDeleteQueryFromTable: function (e) {
      var name = this.getQueryNameFromTable(e);
      this.deleteQueryModal(name);
    },

    selectAndExplainQueryFromTable: function (e) {
      this.selectQueryFromTable(e, false);
      this.explainQuery();
    },

    selectAndRunQueryFromTable: function (e) {
      this.selectQueryFromTable(e, false);
      this.executeQuery();
    },

    selectQueryFromTable: function (e, toggle) {
      var name = this.getQueryNameFromTable(e);
      var self = this;

      if (toggle === undefined) {
        this.toggleQueries();
      }

      // backup the last query
      this.state.lastQuery.query = this.aqlEditor.getValue();
      this.state.lastQuery.bindParam = this.bindParamTableObj;

      this.aqlEditor.setValue(this.getCustomQueryValueByName(name), 1);
      this.fillBindParamTable(this.getCustomQueryParameterByName(name));
      this.updateBindParams();

      this.currentQuery = this.collection.findWhere({name: name});

      if (this.currentQuery) {
        localStorage.setItem('lastOpenQuery', this.currentQuery.get('name'));
      }

      $('#updateCurrentQuery').show();

      // render a button to revert back to last query
      $('#lastQuery').remove();

      $('#queryContent .arangoToolbarTop .pull-left')
        .append('<span id="lastQuery" class="clickable">Previous Query</span>');

      this.breadcrumb(name);

      $('#lastQuery').hide().fadeIn(500)
        .on('click', function () {
          $('#updateCurrentQuery').hide();
          self.aqlEditor.setValue(self.state.lastQuery.query, 1);
          self.fillBindParamTable(self.state.lastQuery.bindParam);
          self.updateBindParams();

          self.collection.each(function (model) {
            model = model.toJSON();

            if (model.value === self.state.lastQuery.query) {
              self.breadcrumb(model.name);
            } else {
              self.breadcrumb();
            }
          });

          $('#lastQuery').fadeOut(500, function () {
            $(this).remove();
          });
        }
      );
    },

    deleteAQL: function (name) {
      var callbackRemove = function (error) {
        if (error) {
          arangoHelper.arangoError('Query', 'Could not delete query.');
        } else {
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

    switchAce: function (e) {
      // check if button is disabled
      var count = $(e.currentTarget).attr('counter');
      var elem = e.currentTarget;

      if ($(elem).hasClass('disabled')) {
        return;
      }

      _.each($(elem).parent().children(), function (child) {
        $(child).removeClass('active');
      });

      var string = $(elem).attr('val');
      $(elem).addClass('active');
      $(elem).text(string.charAt(0).toUpperCase() + string.slice(1));

      // refactor this
      if (string === 'JSON') {
        $('#outputEditor' + count).show();

        $('#outputGraph' + count).hide();
        $('#outputTable' + count).hide();
      } else if (string === 'Table') {
        $('#outputTable' + count).show();

        $('#outputGraph' + count).hide();
        $('#outputEditor' + count).hide();
      } else if (string === 'Graph') {
        $('#outputGraph' + count).show();

        $('#outputTable' + count).hide();
        $('#outputEditor' + count).hide();
      }

      // deselect editors
      this.deselect(ace.edit('outputEditor' + count));
      this.deselect(ace.edit('sentQueryEditor' + count));
      this.deselect(ace.edit('sentBindParamEditor' + count));
    },

    downloadQueryResult: function (e) {
      var count = $(e.currentTarget).attr('counter');
      var editor = ace.edit('sentQueryEditor' + count);
      var query = editor.getValue();

      if (query !== '' || query !== undefined || query !== null) {
        var url;
        if (Object.keys(this.bindParamTableObj).length === 0) {
          url = 'query/result/download/' + encodeURIComponent(btoa(JSON.stringify({ query: query })));
        } else {
          url = 'query/result/download/' + encodeURIComponent(btoa(JSON.stringify({
            query: query,
            bindVars: this.bindParamTableObj
          })));
        }
        arangoHelper.download(url);
      } else {
        arangoHelper.arangoError('Query error', 'could not query result.');
      }
    },

    explainQuery: function () {
      if (this.verifyQueryAndParams()) {
        return;
      }

      this.$(this.outputDiv).prepend(this.outputTemplate.render({
        counter: this.outputCounter,
        type: 'Explain'
      }));

      var counter = this.outputCounter;
      var outputEditor = ace.edit('outputEditor' + counter);
      var sentQueryEditor = ace.edit('sentQueryEditor' + counter);
      var sentBindParamEditor = ace.edit('sentBindParamEditor' + counter);

      sentQueryEditor.getSession().setMode('ace/mode/aql');
      sentQueryEditor.setOption('vScrollBarAlwaysVisible', true);
      sentQueryEditor.setReadOnly(true);
      this.setEditorAutoHeight(sentQueryEditor);

      outputEditor.setReadOnly(true);
      outputEditor.getSession().setMode('ace/mode/json');
      outputEditor.setOption('vScrollBarAlwaysVisible', true);
      this.setEditorAutoHeight(outputEditor);

      sentBindParamEditor.setValue(JSON.stringify(this.bindParamTableObj), 1);
      sentBindParamEditor.setOption('vScrollBarAlwaysVisible', true);
      sentBindParamEditor.getSession().setMode('ace/mode/json');
      sentBindParamEditor.setReadOnly(true);
      this.setEditorAutoHeight(sentBindParamEditor);

      this.fillExplain(outputEditor, sentQueryEditor, counter);
      this.outputCounter++;
    },

    fillExplain: function (outputEditor, sentQueryEditor, counter) {
      sentQueryEditor.setValue(this.aqlEditor.getValue(), 1);

      var self = this;
      var queryData = this.readQueryData();

      if (queryData === 'false') {
        return;
      }

      $('#outputEditorWrapper' + counter + ' .queryExecutionTime').text('');
      this.execPending = false;

      if (queryData) {
        var afterResult = function () {
          $('#outputEditorWrapper' + counter + ' #spinner').remove();
          $('#outputEditor' + counter).css('opacity', '1');
          $('#outputEditorWrapper' + counter + ' .fa-close').show();
          $('#outputEditorWrapper' + counter + ' .switchAce').show();
        };

        $.ajax({
          type: 'POST',
          url: arangoHelper.databaseUrl('/_admin/aardvark/query/explain/'),
          data: queryData,
          contentType: 'application/json',
          processData: false,
          success: function (data) {
            if (data.msg.includes('errorMessage')) {
              self.removeOutputEditor(counter);
              arangoHelper.arangoError('Explain', data.msg);
            } else {
              outputEditor.setValue(data.msg, 1);
              self.deselect(outputEditor);
              $.noty.clearQueue();
              $.noty.closeAll();
              self.handleResult(counter);
            }
            afterResult();
          },
          error: function (data) {
            try {
              var temp = JSON.parse(data.responseText);
              arangoHelper.arangoError('Explain', temp.errorMessage);
            } catch (e) {
              arangoHelper.arangoError('Explain', 'ERROR');
            }
            self.handleResult(counter);
            self.removeOutputEditor(counter);
            afterResult();
          }
        });
      }
    },

    removeOutputEditor: function (counter) {
      $('#outputEditorWrapper' + counter).hide();
      $('#outputEditorWrapper' + counter).remove();
      if ($('.outputEditorWrapper').length === 0) {
        $('#removeResults').hide();
      }
    },

    getCachedQueryAfterRender: function () {
      if (this.renderComplete === false) {
        // get cached query if available
        var queryObject = this.getCachedQuery();
        var self = this;

        if (queryObject !== null && queryObject !== undefined && queryObject !== '') {
          this.aqlEditor.setValue(queryObject.query, 1);

          var queryName = localStorage.getItem('lastOpenQuery');

          if (queryName !== undefined && queryName !== 'undefined') {
            try {
              var query = this.collection.findWhere({name: queryName}).toJSON();
              if (query.value === queryObject.query) {
                self.breadcrumb(queryName);
                $('#updateCurrentQuery').show();
              }
            } catch (ignore) {
            }
          }

          // reset undo history for initial text value
          this.aqlEditor.getSession().setUndoManager(new ace.UndoManager());

          if (queryObject.parameter !== '' || queryObject !== undefined) {
            try {
              // then fill values into input boxes
              self.bindParamTableObj = JSON.parse(queryObject.parameter);

              var key;
              _.each($('#arangoBindParamTable input'), function (element) {
                key = $(element).attr('name');
                $(element).val(self.bindParamTableObj[key]);
              });

              // resave cached query
              self.setCachedQuery(self.aqlEditor.getValue(), JSON.stringify(self.bindParamTableObj));
            } catch (ignore) {}
          }
        }
        this.renderComplete = true;
      }
    },

    getCachedQuery: function () {
      if (Storage !== 'undefined') {
        var cache = localStorage.getItem('cachedQuery');
        if (cache !== undefined) {
          var query = JSON.parse(cache);
          this.currentQuery = query;
          try {
            this.bindParamTableObj = JSON.parse(query.parameter);
          } catch (ignore) {}
          return query;
        }
      }
    },

    setCachedQuery: function (query, vars) {
      if (Storage !== 'undefined') {
        var myObject = {
          query: query,
          parameter: vars
        };
        this.currentQuery = myObject;
        localStorage.setItem('cachedQuery', JSON.stringify(myObject));
      }
    },

    closeResult: function (e) {
      var target = $('#' + $(e.currentTarget).attr('element')).parent();
      $(target).hide('fast', function () {
        $(target).remove();
        if ($('.outputEditorWrapper').length === 0) {
          $('#removeResults').hide();
        }
      });
    },

    fillSelectBoxes: function () {
      // fill select box with # of results
      var querySize = 100;
      var sizeBox = $('#querySize');
      sizeBox.empty();

      [ 100, 250, 500, 1000, 2500, 5000, 10000, 'all' ].forEach(function (value) {
        sizeBox.append('<option value="' + _.escape(value) + '"' +
          (querySize === value ? ' selected' : '') +
          '>' + _.escape(value) + ' results</option>');
      });
    },

    render: function () {
      this.refreshAQL();
      this.renderComplete = false;
      this.$el.html(this.template.render({}));

      this.afterRender();

      if (!this.initDone) {
        // init aql editor width
        this.settings.aqlWidth = $('.aqlEditorWrapper').width();
      }

      this.initDone = true;
      this.renderBindParamTable(true);
    },

    afterRender: function () {
      var self = this;
      this.initAce();
      this.initTables();
      this.fillSelectBoxes();
      this.makeResizeable();
      this.initQueryImport();

      // set height of editor wrapper
      $('.inputEditorWrapper').height($(window).height() / 10 * 5 + 25);
      window.setTimeout(function () {
        self.resize();
      }, 10);
      self.deselect(self.aqlEditor);
    },

    showSpotlight: function (type) {
      var callback, cancelCallback;

      if (type === undefined || type.type === 'click') {
        type = 'aql';
      }

      if (type === 'aql') {
        callback = function (string) {
          this.aqlEditor.insert(string);
          $('#aqlEditor .ace_text-input').focus();
        }.bind(this);

        cancelCallback = function () {
          $('#aqlEditor .ace_text-input').focus();
        };
      } else {
        var focused = $(':focus');
        callback = function (string) {
          var old = $(focused).val();
          $(focused).val(old + string);
          $(focused).focus();
        };

        cancelCallback = function () {
          $(focused).focus();
        };
      }

      window.spotlightView.show(callback, cancelCallback, type);
    },

    resize: function () {
      this.resizeFunction();
    },

    resizeFunction: function () {
      if ($('#toggleQueries1').is(':visible')) {
        this.aqlEditor.resize();
        $('#arangoBindParamTable thead').css('width', $('#bindParamEditor').width());
        $('#arangoBindParamTable thead th').css('width', $('#bindParamEditor').width() / 2);
        $('#arangoBindParamTable tr').css('width', $('#bindParamEditor').width());
        $('#arangoBindParamTable tbody').css('height', $('#aqlEditor').height() - 35);
        $('#arangoBindParamTable tbody').css('width', $('#bindParamEditor').width());
        $('#arangoBindParamTable tbody tr').css('width', $('#bindParamEditor').width());
        $('#arangoBindParamTable tbody td').css('width', $('#bindParamEditor').width() / 2);
      } else {
        this.queryPreview.resize();
        $('#arangoMyQueriesTable thead').css('width', $('#queryTable').width());
        $('#arangoMyQueriesTable thead th').css('width', $('#queryTable').width() / 2);
        $('#arangoMyQueriesTable tr').css('width', $('#queryTable').width());
        $('#arangoMyQueriesTable tbody').css('height', $('#queryTable').height() - 35);
        $('#arangoMyQueriesTable tbody').css('width', $('#queryTable').width());
        $('#arangoMyQueriesTable tbody td').css('width', $('#queryTable').width() / 2);
      }
    },

    makeResizeable: function () {
      var self = this;

      $('.aqlEditorWrapper').resizable({
        resize: function () {
          self.resizeFunction();
          self.settings.aqlWidth = $('.aqlEditorWrapper').width();
        },
        handles: 'e'
      });

      $('.inputEditorWrapper').resizable({
        resize: function () {
          self.resizeFunction();
        },
        handles: 's'
      });

      // one manual start
      this.resizeFunction();
    },

    initTables: function () {
      this.$(this.bindParamId).html(this.table.render({content: this.bindParamTableDesc}));
      this.$(this.myQueriesId).html(this.table.render({content: this.myQueriesTableDesc}));
    },

    checkType: function (val) {
      var type = 'stringtype';

      try {
        val = JSON.parse(val);
        if (val instanceof Array) {
          type = 'arraytype';
        } else {
          type = typeof val + 'type';
        }
      } catch (ignore) {}

      return type;
    },

    updateBindParams: function (e) {
      var id;
      var self = this;

      if (e) {
        id = $(e.currentTarget).attr('name');
        // this.bindParamTableObj[id] = $(e.currentTarget).val()
        this.bindParamTableObj[id] = arangoHelper.parseInput(e.currentTarget);

        var types = [
          'arraytype', 'objecttype', 'booleantype', 'numbertype', 'stringtype'
        ];
        _.each(types, function (type) {
          $(e.currentTarget).removeClass(type);
        });
        $(e.currentTarget).addClass(self.checkType($(e.currentTarget).val()));
      } else {
        _.each($('#arangoBindParamTable input'), function (element) {
          id = $(element).attr('name');
          self.bindParamTableObj[id] = arangoHelper.parseInput(element);
        });
      }
      this.setCachedQuery(this.aqlEditor.getValue(), JSON.stringify(this.bindParamTableObj));

      // fire execute if return was pressed
      if (e) {
        if ((e.ctrlKey || e.metaKey) && e.keyCode === 13) {
          e.preventDefault();
          this.executeQuery();
        }
        if ((e.ctrlKey || e.metaKey) && e.keyCode === 32) {
          e.preventDefault();
          this.showSpotlight('bind');
        }
      }
    },

    parseQuery: function (query) {
      var STATE_NORMAL = 0;
      var STATE_STRING_SINGLE = 1;
      var STATE_STRING_DOUBLE = 2;
      var STATE_STRING_TICK = 3;
      var STATE_COMMENT_SINGLE = 4;
      var STATE_COMMENT_MULTI = 5;
      var STATE_BIND = 6;
      var STATE_STRING_BACKTICK = 7;

      query += ' ';
      var self = this;
      var start;
      var state = STATE_NORMAL;
      var n = query.length;
      var i, c;

      var bindParams = [];

      for (i = 0; i < n; ++i) {
        c = query.charAt(i);

        switch (state) {
          case STATE_NORMAL:
            if (c === '@') {
              state = STATE_BIND;
              start = i;
            } else if (c === "'") {
              state = STATE_STRING_SINGLE;
            } else if (c === '"') {
              state = STATE_STRING_DOUBLE;
            } else if (c === '`') {
              state = STATE_STRING_TICK;
            } else if (c === '´') {
              state = STATE_STRING_BACKTICK;
            } else if (c === '/') {
              if (i + 1 < n) {
                if (query.charAt(i + 1) === '/') {
                  state = STATE_COMMENT_SINGLE;
                  ++i;
                } else if (query.charAt(i + 1) === '*') {
                  state = STATE_COMMENT_MULTI;
                  ++i;
                }
              }
            }
            break;
          case STATE_COMMENT_SINGLE:
            if (c === '\r' || c === '\n') {
              state = STATE_NORMAL;
            }
            break;
          case STATE_COMMENT_MULTI:
            if (c === '*') {
              if (i + 1 <= n && query.charAt(i + 1) === '/') {
                state = STATE_NORMAL;
                ++i;
              }
            }
            break;
          case STATE_STRING_SINGLE:
            if (c === '\\') {
              ++i;
            } else if (c === "'") {
              state = STATE_NORMAL;
            }
            break;
          case STATE_STRING_DOUBLE:
            if (c === '\\') {
              ++i;
            } else if (c === '"') {
              state = STATE_NORMAL;
            }
            break;
          case STATE_STRING_TICK:
            if (c === '`') {
              state = STATE_NORMAL;
            }
            break;
          case STATE_STRING_BACKTICK:
            if (c === '´') {
              state = STATE_NORMAL;
            }
            break;
          case STATE_BIND:
            if (!/^[@a-zA-Z0-9_]+$/.test(c)) {
              bindParams.push(query.substring(start, i));
              state = STATE_NORMAL;
              start = undefined;
            }
            break;
        }
      }

      var match;
      _.each(bindParams, function (v, k) {
        match = v.match(self.bindParamRegExp);

        if (match) {
          bindParams[k] = match[1];
        }
      });

      return {
        query: query,
        bindParams: bindParams
      };
    },

    checkForNewBindParams: function () {
      var self = this;
      // Remove comments
      var foundBindParams = this.parseQuery(this.aqlEditor.getValue()).bindParams;

      var newObject = {};
      _.each(foundBindParams, function (word) {
        if (self.bindParamTableObj[word]) {
          newObject[word] = self.bindParamTableObj[word];
        } else {
          newObject[word] = '';
        }
      });

      Object.keys(foundBindParams).forEach(function (keyNew) {
        Object.keys(self.bindParamTableObj).forEach(function (keyOld) {
          if (keyNew === keyOld) {
            newObject[keyNew] = self.bindParamTableObj[keyOld];
          }
        });
      });
      self.bindParamTableObj = newObject;
    },

    renderBindParamTable: function (init) {
      $('#arangoBindParamTable tbody').html('');

      if (init) {
        this.getCachedQuery();
      }

      var counter = 0;

      _.each(this.bindParamTableObj, function (val, key) {
        $('#arangoBindParamTable tbody').append(
          '<tr>' +
          '<td>' + key + '</td>' +
          '<td><input name=' + key + ' type="text"></input></td>' +
          '</tr>'
        );
        counter++;
        _.each($('#arangoBindParamTable input'), function (element) {
          if ($(element).attr('name') === key) {
            if (val instanceof Array) {
              $(element).val(JSON.stringify(val)).addClass('arraytype');
            } else if (typeof val === 'object') {
              $(element).val(JSON.stringify(val)).addClass(typeof val + 'type');
            } else {
              $(element).val(val).addClass(typeof val + 'type');
            }
          }
        });
      });
      if (counter === 0) {
        $('#arangoBindParamTable tbody').append(
          '<tr class="noBgColor">' +
          '<td>No bind parameters defined.</td>' +
          '<td></td>' +
          '</tr>'
        );
      }
    },

    fillBindParamTable: function (object) {
      _.each(object, function (val, key) {
        _.each($('#arangoBindParamTable input'), function (element) {
          if ($(element).attr('name') === key) {
            $(element).val(val);
          }
        });
      });
    },

    initAce: function () {
      var self = this;

      // init aql editor
      this.aqlEditor = ace.edit('aqlEditor');
      this.aqlEditor.getSession().setMode('ace/mode/aql');
      this.aqlEditor.setFontSize('10pt');
      this.aqlEditor.setShowPrintMargin(false);

      this.bindParamAceEditor = ace.edit('bindParamAceEditor');
      this.bindParamAceEditor.getSession().setMode('ace/mode/json');
      this.bindParamAceEditor.setFontSize('10pt');
      this.bindParamAceEditor.setShowPrintMargin(false);

      this.bindParamAceEditor.getSession().on('change', function () {
        try {
          self.bindParamTableObj = JSON.parse(self.bindParamAceEditor.getValue());
          self.allowParamToggle = true;
          self.setCachedQuery(self.aqlEditor.getValue(), JSON.stringify(self.bindParamTableObj));
        } catch (e) {
          if (self.bindParamAceEditor.getValue() === '') {
            _.each(self.bindParamTableObj, function (v, k) {
              self.bindParamTableObj[k] = '';
            });
            self.allowParamToggle = true;
          } else {
            self.allowParamToggle = false;
          }
        }
      });

      this.aqlEditor.getSession().on('change', function () {
        self.checkForNewBindParams();
        self.renderBindParamTable();
        if (self.initDone) {
          self.setCachedQuery(self.aqlEditor.getValue(), JSON.stringify(self.bindParamTableObj));
        }
        self.bindParamAceEditor.setValue(JSON.stringify(self.bindParamTableObj, null, '\t'), 1);
        $('#aqlEditor .ace_text-input').focus();

        self.resize();
      });

      this.aqlEditor.commands.addCommand({
        name: 'togglecomment',
        bindKey: {win: 'Ctrl-Shift-C', linux: 'Ctrl-Shift-C', mac: 'Command-Shift-C'},
        exec: function (editor) {
          editor.toggleCommentLines();
        },
        multiSelectAction: 'forEach'
      });

      this.aqlEditor.commands.addCommand({
        name: 'executeQuery',
        bindKey: {win: 'Ctrl-Return', mac: 'Command-Return', linux: 'Ctrl-Return'},
        exec: function () {
          self.executeQuery();
        }
      });

      this.aqlEditor.commands.addCommand({
        name: 'executeSelectedQuery',
        bindKey: {win: 'Ctrl-Alt-Return', mac: 'Command-Alt-Return', linux: 'Ctrl-Alt-Return'},
        exec: function () {
          self.executeQuery(undefined, true);
        }
      });

      this.aqlEditor.commands.addCommand({
        name: 'saveQuery',
        bindKey: {win: 'Ctrl-Shift-S', mac: 'Command-Shift-S', linux: 'Ctrl-Shift-S'},
        exec: function () {
          self.addAQL();
        }
      });

      this.aqlEditor.commands.addCommand({
        name: 'explainQuery',
        bindKey: {win: 'Ctrl-Shift-Return', mac: 'Command-Shift-Return', linux: 'Ctrl-Shift-Return'},
        exec: function () {
          self.explainQuery();
        }
      });

      this.aqlEditor.commands.addCommand({
        name: 'togglecomment',
        bindKey: {win: 'Ctrl-Shift-C', linux: 'Ctrl-Shift-C', mac: 'Command-Shift-C'},
        exec: function (editor) {
          editor.toggleCommentLines();
        },
        multiSelectAction: 'forEach'
      });

      this.aqlEditor.commands.addCommand({
        name: 'showSpotlight',
        bindKey: {win: 'Ctrl-Space', mac: 'Ctrl-Space', linux: 'Ctrl-Space'},
        exec: function () {
          self.showSpotlight();
        }
      });

      this.queryPreview = ace.edit('queryPreview');
      this.queryPreview.getSession().setMode('ace/mode/aql');
      this.queryPreview.setReadOnly(true);
      this.queryPreview.setFontSize('13px');

      // auto focus this editor
      $('#aqlEditor .ace_text-input').focus();
    },

    updateQueryTable: function () {
      var self = this;
      this.updateLocalQueries();

      this.myQueriesTableDesc.rows = this.customQueries;
      _.each(this.myQueriesTableDesc.rows, function (k) {
        k.secondRow = '<span class="spanWrapper">' +
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

      function compare (a, b) {
        var x;
        if (a.name < b.name) {
          x = -1;
        } else if (a.name > b.name) {
          x = 1;
        } else {
          x = 0;
        }
        return x;
      }

      this.myQueriesTableDesc.rows.sort(compare);

      _.each(this.queries, function (val) {
        if (val.hasOwnProperty('parameter')) {
          delete val.parameter;
        }
        self.myQueriesTableDesc.rows.push({
          name: val.name,
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
        if ($('#modalButton1').html() === 'Update') {
          this.saveAQL();
        }
      }
      this.checkSaveName();
    },

    addAQL: function () {
      // update queries first, before showing
      this.refreshAQL(true);

      // render options
      this.createCustomQueryModal();
      setTimeout(function () {
        $('#new-query-name').focus();
      }, 500);
    },

    updateAQL: function () {
      var content = this.aqlEditor.getValue();
      var queryName = localStorage.getItem('lastOpenQuery');
      var query = this.collection.findWhere({name: queryName});
      if (query) {
        query.set('value', content);
        var callback = function (error) {
          if (error) {
            arangoHelper.arangoError('Query', 'Could not save query');
          } else {
            var self = this;
            arangoHelper.arangoNotification('Queries', 'Saved query ' + queryName);
            this.collection.fetch({
              success: function () {
                self.updateLocalQueries();
              }
            });
          }
        }.bind(this);
        this.collection.saveCollectionQueries(callback);
      }

      this.refreshAQL(true);
    },

    createAQL: function () {
      localStorage.setItem('lastOpenQuery', undefined);
      this.aqlEditor.setValue('');

      this.refreshAQL(true);

      this.breadcrumb();
      $('#updateCurrentQuery').hide();
    },

    createCustomQueryModal: function () {
      var buttons = [];
      var tableContent = [];

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
              msg: 'No query name given.'
            }
          ]
        )
      );
      buttons.push(
        window.modalView.createSuccessButton('Save', this.saveAQL.bind(this))
      );
      window.modalView.show('modalTable.ejs', 'Save Query', buttons, tableContent, undefined, undefined,
        {'keyup #new-query-name': this.listenKey.bind(this)});
    },

    checkSaveName: function () {
      var saveName = $('#new-query-name').val();
      if (saveName === 'Insert Query') {
        $('#new-query-name').val('');
        return;
      }

      // check for invalid query names, if present change the box-shadow to red
      // and disable the save functionality
      var found = this.customQueries.some(function (query) {
        return query.name === saveName;
      });
      if (found) {
        $('#modalButton1').removeClass('button-success');
        $('#modalButton1').addClass('button-warning');
        $('#modalButton1').text('Update');
      } else {
        $('#modalButton1').removeClass('button-warning');
        $('#modalButton1').addClass('button-success');
        $('#modalButton1').text('Save');
      }
    },

    saveAQL: function (e) {
      if (e) {
        e.stopPropagation();
      }

      // update queries first, before writing
      this.refreshAQL();

      var saveName = $('#new-query-name').val();
      var bindVars = this.bindParamTableObj;

      if ($('#new-query-name').hasClass('invalid-input')) {
        return;
      }

      // Heiko: Form-Validator - illegal query name
      if (saveName.trim() === '') {
        return;
      }

      var content = this.aqlEditor.getValue();
        // check for already existing entry
      var quit = false;
      _.each(this.customQueries, function (v) {
        if (v.name === saveName) {
          v.value = content;
          quit = true;
          return;
        }
      });

      if (quit === true) {
        // Heiko: Form-Validator - name already taken
        // Update model and save
        this.collection.findWhere({name: saveName}).set('value', content);
      } else {
        if (bindVars === '' || bindVars === undefined) {
          bindVars = '{}';
        }

        if (typeof bindVars === 'string') {
          try {
            bindVars = JSON.parse(bindVars);
          } catch (err) {
            arangoHelper.arangoError('Query', 'Could not parse bind parameter');
          }
        }
        this.collection.add({
          name: saveName,
          parameter: bindVars,
          value: content
        });
      }

      var callback = function (error) {
        if (error) {
          arangoHelper.arangoError('Query', 'Could not save query');
        } else {
          var self = this;
          this.collection.fetch({
            success: function () {
              self.updateLocalQueries();
              $('#updateCurrentQuery').show();

              self.breadcrumb(saveName);
            }
          });
        }
      }.bind(this);
      this.collection.saveCollectionQueries(callback);
      window.modalView.hide();
    },

    breadcrumb: function (name) {
      window.setTimeout(function () {
        if (name) {
          $('#subNavigationBar .breadcrumb').html(
            'Query: ' + name
          );
        } else {
          $('#subNavigationBar .breadcrumb').html('');
        }
      }, 50);
    },

    verifyQueryAndParams: function () {
      var quit = false;

      if (this.aqlEditor.getValue().length === 0) {
        arangoHelper.arangoError('Query', 'Your query is empty');
        quit = true;
      }

      var keys = [];
      _.each(this.bindParamTableObj, function (val, key) {
        if (val === '') {
          quit = true;
          keys.push(key);
        }
      });
      if (keys.length > 0) {
        arangoHelper.arangoError('Bind Parameter', JSON.stringify(keys) + ' not defined.');
      }

      return quit;
    },

    executeQuery: function (e, selected) {
      if (this.verifyQueryAndParams()) {
        return;
      }

      this.$(this.outputDiv).prepend(this.outputTemplate.render({
        counter: this.outputCounter,
        type: 'Query'
      }));

      $('#outputEditorWrapper' + this.outputCounter).hide();
      $('#outputEditorWrapper' + this.outputCounter).show('fast');

      var counter = this.outputCounter;
      var outputEditor = ace.edit('outputEditor' + counter);
      var sentQueryEditor = ace.edit('sentQueryEditor' + counter);
      var sentBindParamEditor = ace.edit('sentBindParamEditor' + counter);

      sentQueryEditor.getSession().setMode('ace/mode/aql');
      sentQueryEditor.setOption('vScrollBarAlwaysVisible', true);
      sentQueryEditor.setFontSize('13px');
      sentQueryEditor.setReadOnly(true);
      this.setEditorAutoHeight(sentQueryEditor);

      outputEditor.setFontSize('13px');
      outputEditor.getSession().setMode('ace/mode/json');
      outputEditor.setReadOnly(true);
      outputEditor.setOption('vScrollBarAlwaysVisible', true);
      outputEditor.setShowPrintMargin(false);
      this.setEditorAutoHeight(outputEditor);

      sentBindParamEditor.setValue(JSON.stringify(this.bindParamTableObj), 1);
      sentBindParamEditor.setOption('vScrollBarAlwaysVisible', true);
      sentBindParamEditor.getSession().setMode('ace/mode/json');
      sentBindParamEditor.setReadOnly(true);
      this.setEditorAutoHeight(sentBindParamEditor);

      this.fillResult(outputEditor, sentQueryEditor, counter, selected);
      this.outputCounter++;
    },

    readQueryData: function (selected) {
      // var selectedText = this.aqlEditor.session.getTextRange(this.aqlEditor.getSelectionRange())
      var sizeBox = $('#querySize');
      var data = {
        id: 'currentFrontendQuery'
      };

      if (selected) {
        data.query = this.aqlEditor.getSelectedText();
      } else {
        data.query = this.aqlEditor.getValue();
      }
      if (data.query.length === 0) {
        if (selected) {
          arangoHelper.arangoError('Query', 'Your query selection is empty!');
        } else {
          arangoHelper.arangoError('Query', 'Your query is empty!');
        }
        data = false;
      } else {
        if (sizeBox.val() === 'all') {
          data.batchSize = 1000000;
        } else {
          data.batchSize = parseInt(sizeBox.val(), 10);
        }

        if (Object.keys(this.bindParamTableObj).length > 0) {
          data.bindVars = this.bindParamTableObj;
        }
      }

      return JSON.stringify(data);
    },

    fillResult: function (outputEditor, sentQueryEditor, counter, selected) {
      var self = this;

      var queryData = this.readQueryData(selected);

      if (queryData === 'false') {
        return;
      }

      if (queryData) {
        sentQueryEditor.setValue(self.aqlEditor.getValue(), 1);

        $.ajax({
          type: 'POST',
          url: arangoHelper.databaseUrl('/_api/cursor'),
          headers: {
            'x-arango-async': 'store'
          },
          data: queryData,
          contentType: 'application/json',
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
            } catch (e) {
              arangoHelper.arangoError('Query error', 'ERROR');
            }
            self.handleResult(counter);
          }
        });
      }
    },

    handleResult: function () {
      var self = this;
      window.progressView.hide();
      $('#removeResults').show();

      // refocus ace
      window.setTimeout(function () {
        self.aqlEditor.focus();
      }, 300);

      $('.centralRow').animate({ scrollTop: $('#queryContent').height() }, 'fast');
    },

    setEditorAutoHeight: function (editor) {
      // ace line height = 17px
      var winHeight = $('.centralRow').height();
      var maxLines = (winHeight - 250) / 17;

      editor.setOptions({
        maxLines: maxLines,
        minLines: 10
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

    queryCallbackFunction: function (queryID, outputEditor, counter) {
      var self = this;

      var cancelRunningQuery = function (id, counter) {
        $.ajax({
          url: arangoHelper.databaseUrl('/_api/job/' + encodeURIComponent(id) + '/cancel'),
          type: 'PUT',
          success: function () {
            window.clearTimeout(self.checkQueryTimer);
            $('#outputEditorWrapper' + counter).remove();
            arangoHelper.arangoNotification('Query', 'Query canceled.');
          }
        });
      };

      $('#outputEditorWrapper' + counter + ' #cancelCurrentQuery').bind('click', function () {
        cancelRunningQuery(queryID, counter);
      });

      $('#outputEditorWrapper' + counter + ' #copy2aqlEditor').bind('click', function () {
        if (!$('#toggleQueries1').is(':visible')) {
          self.toggleQueries();
        }

        var aql = ace.edit('sentQueryEditor' + counter).getValue();
        var bindParam = JSON.parse(ace.edit('sentBindParamEditor' + counter).getValue());
        self.aqlEditor.setValue(aql, 1);
        self.deselect(self.aqlEditor);
        if (Object.keys(bindParam).length > 0) {
          self.bindParamTableObj = bindParam;
          self.setCachedQuery(self.aqlEditor.getValue(), JSON.stringify(self.bindParamTableObj));

          if ($('#bindParamEditor').is(':visible')) {
            self.renderBindParamTable();
          } else {
            self.bindParamAceEditor.setValue(JSON.stringify(bindParam), 1);
            self.deselect(self.bindParamAceEditor);
          }
        }
        $('.centralRow').animate({ scrollTop: 0 }, 'fast');
        self.resize();
      });

      this.execPending = false;

      var warningsFunc = function (data) {
        var warnings = '';
        if (data.extra && data.extra.warnings && data.extra.warnings.length > 0) {
          warnings += 'Warnings:' + '\r\n\r\n';
          data.extra.warnings.forEach(function (w) {
            warnings += '[' + w.code + "], '" + w.message + "'\r\n";
          });
        }
        if (warnings !== '') {
          warnings += '\r\n' + 'Result:' + '\r\n\r\n';
        }
        outputEditor.setValue(warnings + JSON.stringify(data.result, undefined, 2), 1);
        outputEditor.getSession().setScrollTop(0);
      };

      var fetchQueryResult = function (data) {
        warningsFunc(data);
        window.progressView.hide();

        var result = self.analyseQuery(data.result);
        // console.log('Using ' + result.defaultType + ' as data format.');
        if (result.defaultType === 'table') {
          $('#outputEditorWrapper' + counter + ' .arangoToolbarTop').after(
            '<div id="outputTable' + counter + '" class="outputTable"></div>'
          );
          $('#outputTable' + counter).show();
          self.renderOutputTable(result, counter);

          // apply max height for table output dynamically
          var maxHeight = $('.centralRow').height() - 250;
          $('.outputEditorWrapper .tableWrapper').css('max-height', maxHeight);

          $('#outputEditor' + counter).hide();
        } else if (result.defaultType === 'graph') {
          $('#outputEditorWrapper' + counter + ' .arangoToolbarTop').after('<div id="outputGraph' + counter + '"></div>');
          $('#outputGraph' + counter).show();
          self.renderOutputGraph(result, counter);

          $('#outputEditor' + counter).hide();
        }

        // add active class to choosen display method
        $('#' + result.defaultType + '-switch').addClass('active').css('display', 'inline');

        var appendSpan = function (value, icon, css) {
          if (!css) {
            css = '';
          }
          $('#outputEditorWrapper' + counter + ' .arangoToolbarTop .pull-left').append(
            '<span class="' + css + '"><i class="fa ' + icon + '"></i><i class="iconText">' + value + '</i></span>'
          );
        };

        $('#outputEditorWrapper' + counter + ' .pull-left #spinner').remove();

        var time = '-';
        if (data && data.extra && data.extra.stats) {
          time = data.extra.stats.executionTime.toFixed(3) + ' s';
        }
        appendSpan(
          data.result.length + ' elements', 'fa-calculator'
        );
        appendSpan(time, 'fa-clock-o');

        if (data.extra) {
          if (data.extra.stats) {
            if (data.extra.stats.writesExecuted > 0 || data.extra.stats.writesIgnored > 0) {
              appendSpan(
                data.extra.stats.writesExecuted + ' writes', 'fa-check-circle positive'
              );
              if (data.extra.stats.writesIgnored === 0) {
                appendSpan(
                  data.extra.stats.writesIgnored + ' writes ignored', 'fa-check-circle positive', 'additional'
                );
              } else {
                appendSpan(
                  data.extra.stats.writesIgnored + ' writes ignored', 'fa-exclamation-circle warning', 'additional'
                );
              }
            }
          }
        }

        $('#outputEditorWrapper' + counter + ' .switchAce').show();
        $('#outputEditorWrapper' + counter + ' .fa-close').show();
        $('#outputEditor' + counter).css('opacity', '1');
        $('#outputEditorWrapper' + counter + ' #downloadQueryResult').show();
        $('#outputEditorWrapper' + counter + ' #copy2aqlEditor').show();
        $('#outputEditorWrapper' + counter + ' #cancelCurrentQuery').remove();

        self.setEditorAutoHeight(outputEditor);
        self.deselect(outputEditor);

        // when finished send a delete req to api (free db space)
        if (data.id) {
          $.ajax({
            url: arangoHelper.databaseUrl('/_api/cursor/' + encodeURIComponent(data.id)),
            type: 'DELETE'
          });
        }
      };

      // check if async query is finished
      var checkQueryStatus = function () {
        $.ajax({
          type: 'PUT',
          url: arangoHelper.databaseUrl('/_api/job/' + encodeURIComponent(queryID)),
          contentType: 'application/json',
          processData: false,
          success: function (data, textStatus, xhr) {
            // query finished, now fetch results
            if (xhr.status === 201) {
              fetchQueryResult(data);
            } else if (xhr.status === 204) {
            // query not ready yet, retry
              self.checkQueryTimer = window.setTimeout(function () {
                checkQueryStatus();
              }, 500);
            }
          },
          error: function (resp) {
            var error;

            try {
              if (resp.statusText === 'Gone') {
                arangoHelper.arangoNotification('Query', 'Query execution aborted.');
                self.removeOutputEditor(counter);
                return;
              }

              error = JSON.parse(resp.responseText);
              arangoHelper.arangoError('Query', error.errorMessage);
              if (error.errorMessage) {
                if (error.errorMessage.match(/\d+:\d+/g) !== null) {
                  self.markPositionError(
                    error.errorMessage.match(/'.*'/g)[0],
                    error.errorMessage.match(/\d+:\d+/g)[0]
                  );
                } else {
                  self.markPositionError(
                    error.errorMessage.match(/\(\w+\)/g)[0]
                  );
                }
                self.removeOutputEditor(counter);
              }
            } catch (e) {
              self.removeOutputEditor(counter);
              if (error.code === 409) {
                return;
              }
              if (error.code !== 400 && error.code !== 404 && error.code !== 500) {
                arangoHelper.arangoNotification('Query', 'Successfully aborted.');
              }
            }

            window.progressView.hide();
          }
        });
      };
      checkQueryStatus();
    },

    analyseQuery: function (result) {
      var toReturn = {
        defaultType: null,
        original: result,
        modified: null
      };

      var found = false;

      // check if result could be displayed as graph
      // case a) result has keys named vertices and edges
      if (result[0]) {
        if (result[0].vertices && result[0].edges) {
          var hitsa = 0;
          var totala = 0;

          _.each(result, function (obj) {
            if (obj.edges) {
              _.each(obj.edges, function (edge) {
                if (edge !== null) {
                  if (edge._from && edge._to) {
                    hitsa++;
                  }
                  totala++;
                }
              });
            }
          });

          var percentagea = hitsa / totala * 100;

          if (percentagea >= 95) {
            found = true;
            toReturn.defaultType = 'graph';
            toReturn.graphInfo = 'object';
          }
        } else {
          // case b) 95% have _from and _to attribute
          var hitsb = 0;
          var totalb = result.length;

          _.each(result, function (obj) {
            if (obj._from && obj._to) {
              hitsb++;
            }
          });

          var percentageb = hitsb / totalb * 100;

          if (percentageb >= 95) {
            found = true;
            toReturn.defaultType = 'graph';
            toReturn.graphInfo = 'array';
            // then display as graph
          }
        }
      }

      // check if result could be displayed as table
      if (!found) {
        var maxAttributeCount = 0;
        var check = true;
        var length;
        var attributes = {};

        if (result.length <= 1) {
          check = false;
        }

        if (check) {
          _.each(result, function (obj) {
            length = _.keys(obj).length;

            if (length > maxAttributeCount) {
              maxAttributeCount = length;
            }

            _.each(obj, function (value, key) {
              if (attributes[key]) {
                attributes[key] = attributes[key] + 1;
              } else {
                attributes[key] = 1;
              }
            });
          });

          var rate;

          _.each(attributes, function (val, key) {
            rate = (val / result.length) * 100;

            if (check !== false) {
              if (rate <= 95) {
                check = false;
              }
            }
          });
        }

        if (check) {
          found = true;
          toReturn.defaultType = 'table';
        }
      }

      if (!found) {
      // if all check fails, then just display as json
        toReturn.defaultType = 'json';
      }

      return toReturn;
    },

    markPositionError: function (text, pos) {
      var row;

      if (pos) {
        row = pos.split(':')[0];
        text = text.substr(1, text.length - 2);
      }

      var found = this.aqlEditor.find(text);

      if (!found && pos) {
        this.aqlEditor.selection.moveCursorToPosition({row: row, column: 0});
        this.aqlEditor.selection.selectLine();
      }
      window.setTimeout(function () {
        $('.ace_start').first().css('background', 'rgba(255, 129, 129, 0.7)');
      }, 100);
    },

    refreshAQL: function () {
      var self = this;

      var callback = function (error) {
        if (error) {
          arangoHelper.arangoError('Query', 'Could not reload Queries');
        } else {
          self.updateLocalQueries();
          self.updateQueryTable();
        }
      };

      var originCallback = function () {
        self.getSystemQueries(callback);
      };

      this.getAQL(originCallback);
    },

    getSystemQueries: function (callback) {
      var self = this;

      $.ajax({
        type: 'GET',
        cache: false,
        url: 'js/arango/aqltemplates.json',
        contentType: 'application/json',
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
          arangoHelper.arangoNotification('Query', 'Error while loading system templates');
        }
      });
    },

    updateLocalQueries: function () {
      var self = this;
      this.customQueries = [];

      this.collection.each(function (model) {
        self.customQueries.push({
          name: model.get('name'),
          value: model.get('value'),
          parameter: model.get('parameter')
        });
      });
    },

    renderOutputTable: function (data, counter) {
      var tableDescription = {
        id: 'outputTableData' + counter,
        titles: [],
        rows: []
      };

      var first = true;
      var part = [];
      // self.tableDescription.rows.push(;
      _.each(data.original, function (obj) {
        if (first === true) {
          tableDescription.titles = Object.keys(obj);
          first = false;
        }
        _.each(obj, function (val) {
          if (typeof val === 'object') {
            val = JSON.stringify(val);
          }
          part.push(val);
        });
        tableDescription.rows.push(part);
        part = [];
      });

      $('#outputTable' + counter).append(this.table.render({content: tableDescription}));
    },

    renderOutputGraph: function (data, counter) {
      this.graphViewer = new window.GraphViewer({
        name: undefined,
        documentStore: window.App.arangoDocumentStore,
        collection: new window.GraphCollection(),
        userConfig: window.App.userConfig,
        id: '#outputGraph' + counter,
        data: data
      });
      this.graphViewer.renderAQL();
    },

    getAQL: function (originCallback) {
      var self = this;

      this.collection.fetch({
        success: function () {
          self.getCachedQueryAfterRender();

          // old storage method
          var item = localStorage.getItem('customQueries');
          if (item) {
            var queries = JSON.parse(item);
            // save queries in user collections extra attribute
            _.each(queries, function (oldQuery) {
              self.collection.add({
                value: oldQuery.value,
                name: oldQuery.name
              });
            });

            var callback = function (error) {
              if (error) {
                arangoHelper.arangoError(
                  'Custom Queries',
                  'Could not import old local storage queries'
                );
              } else {
                localStorage.removeItem('customQueries');
              }
            };
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
