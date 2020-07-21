/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, $, L, setTimeout, sessionStorage, ace, Storage, window, _, btoa */
/* global frontendConfig, _, arangoHelper, numeral, templateEngine, Joi, Noty */

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
    maps: {},

    allowUpload: false,
    renderComplete: false,
    loadedCachedQuery: false,

    customQueries: [],
    cachedQueries: {},
    queriesHistory: {},
    graphViewers: [],
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
    bindParamMode: 'table',

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
      'click #profileQuery': 'profileQuery',
      'click #debugQuery': 'debugDownloadDialog',
      'click #clearQuery': 'clearQuery',
      'click .outputEditorWrapper #downloadQueryResult': 'downloadQueryResult',
      'click .outputEditorWrapper #downloadCsvResult': 'downloadCsvResult',
      'click .outputEditorWrapper .switchAce span': 'switchAce',
      'click .outputEditorWrapper .closeResult': 'closeResult',
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
      'click .closeProfile': 'closeProfile',
      'keydown #arangoBindParamTable input': 'updateBindParams',
      'change #arangoBindParamTable input': 'updateBindParams',
      'change #querySize': 'storeQuerySize',
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

    closeProfile: function (e) {
      var count = $(e.currentTarget).parent().attr('counter');

      _.each($('.queryProfile'), function (elem) {
        if ($(elem).attr('counter') === count) {
          $(elem).fadeOut('fast').remove();
        }
      });
    },

    storeQuerySize: function (e) {
      if (typeof (Storage) !== 'undefined') {
        sessionStorage.setItem('querySize', $(e.currentTarget).val());
      }
    },

    restoreQuerySize: function () {
      if (typeof (Storage) !== 'undefined') {
        if (sessionStorage.getItem('querySize')) {
          $('#querySize').val(sessionStorage.getItem('querySize'));
        }
      }
    },

    toggleBindParams: function () {
      if (this.allowParamToggle) {
        $('#bindParamEditor').toggle();
        $('#bindParamAceEditor').toggle();

        if ($('#switchTypes').text() === 'JSON') {
          this.bindParamMode = 'json';
          $('#switchTypes').text('Table');
          this.updateQueryTable();
          this.bindParamAceEditor.setValue(JSON.stringify(this.bindParamTableObj, null, '\t'), 1);
          this.deselect(this.bindParamAceEditor);
        } else {
          this.bindParamMode = 'table';
          $('#switchTypes').text('JSON');
          this.renderBindParamTable();
        }
        this.setCachedQuery(this.aqlEditor.getValue(), JSON.stringify(this.bindParamTableObj));
      } else {
        arangoHelper.arangoError('Bind parameters', 'Could not parse bind parameters');
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

      if (self.allowUpload) {
        $('#confirmQueryImport').removeClass('disabled');
      }

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
      var self = this;
      this.cachedQueries = {};
      this.queriesHistory = {};

      _.each($('.outputEditorWrapper'), function (v) {
        self.closeAceResults(v.id.replace(/^\D+/g, ''));
      });
    },

    removeInputEditors: function () {
      this.closeAceResults(null, $('#aqlEditor'));
      this.closeAceResults(null, $('#bindParamAceEditor'));
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
      var self = this;

      $.ajax({
        type: 'GET',
        url: 'whoAmI?_=' + Date.now(),
        success: function (data) {
          var name = data.user;

          if (name === null || name === false) {
            name = 'root';
          }
          if (frontendConfig.ldapEnabled) {
            self.collection.downloadLocalQueries();
          } else {
            var url = 'query/download/' + encodeURIComponent(name);
            arangoHelper.download(url);
          }
        }
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

          if (sessionStorage.getItem('lastOpenQuery') !== 'undefined') {
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
        'explainQuery', 'profileQuery', 'debugQuery', 'importQuery', 'exportQuery'
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

      try {
        this.queryPreview.setValue(this.getCustomQueryValueByName(name), 1);
      } catch (e) {
        this.queryPreview.setValue('Invalid query name', 1);
        arangoHelper.arangoError('Query', 'Invalid query name');
        throw (e);
      }

      this.deselect(this.queryPreview);
    },

    getQueryNameFromTable: function (e) {
      var name;
      if ($(e.currentTarget).is('tr')) {
        name = arangoHelper.escapeHtml($(e.currentTarget).children().first().text());
      } else if ($(e.currentTarget).is('span')) {
        name = arangoHelper.escapeHtml($(e.currentTarget).parent().parent().prev().text());
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

      var lastQueryName = sessionStorage.getItem('lastOpenQuery');
      // backup the last query
      this.state.lastQuery.query = this.aqlEditor.getValue();
      this.state.lastQuery.bindParam = this.bindParamTableObj;

      try {
        this.aqlEditor.setValue(this.getCustomQueryValueByName(name), 1);
        this.fillBindParamTable(this.getCustomQueryParameterByName(name));
      } catch (e) {
        arangoHelper.arangoError('Query', 'Invalid query name');
        throw (e);
      }
      this.updateBindParams();

      this.currentQuery = this.collection.findWhere({name: name});

      if (this.currentQuery) {
        sessionStorage.setItem('lastOpenQuery', this.currentQuery.get('name'));
      }

      $('#updateCurrentQuery').show();

      // render a button to revert back to last query
      $('#lastQuery').remove();

      if (lastQueryName !== name) {
        $('#queryContent .arangoToolbarTop .pull-left')
          .append('<span id="lastQuery" class="clickable">Previous Query</span>');

        this.breadcrumb(name);
      }

      $('#lastQuery').hide().fadeIn(500).on('click', function () {
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
      });
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
        $('#outputGeo' + count).hide();
      } else if (string === 'Table') {
        $('#outputTable' + count).show();

        $('#outputGraph' + count).hide();
        $('#outputEditor' + count).hide();
        $('#outputGeo' + count).hide();
      } else if (string === 'Graph') {
        $('#outputGraph' + count).show();

        $('#outputTable' + count).hide();
        $('#outputEditor' + count).hide();
        $('#outputGeo' + count).hide();
      } else if (string === 'Geo') {
        $('#outputGeo' + count).show();

        $('#outputGraph' + count).hide();
        $('#outputTable' + count).hide();
        $('#outputEditor' + count).hide();
      }

      // deselect editors
      this.deselect(ace.edit('outputEditor' + count));
    },

    downloadQueryResult: function (e) {
      var counter = $(e.currentTarget).attr('counter');
      var query = this.queriesHistory[counter].sentQuery;

      if (query !== '' && query !== undefined && query !== null) {
        var url;
        if (Object.keys(this.queriesHistory[counter].bindParam).length === 0) {
          url = 'query/result/download/' + encodeURIComponent(btoa(JSON.stringify({ query: query })));
        } else {
          url = 'query/result/download/' + encodeURIComponent(btoa(JSON.stringify({
            query: query,
            bindVars: this.queriesHistory[counter].bindParam
          })));
        }
        arangoHelper.download(url);
      } else {
        arangoHelper.arangoError('Query error', 'Could not download the result.');
      }
    },

    profileQuery: function () {
      this.explainQuery(true);
    },

    explainQuery: function (profile) {
      if (profile !== true) {
        profile = false;
      } else {
        profile = true;
      }

      if (this.verifyQueryAndParams()) {
        return;
      }

      this.lastSentQueryString = this.aqlEditor.getValue();

      var type = 'Explain';
      if (profile) {
        type = 'Profile';
      }

      this.$(this.outputDiv).prepend(this.outputTemplate.render({
        counter: this.outputCounter,
        type: type
      }));

      var counter = this.outputCounter;
      var outputEditor = ace.edit('outputEditor' + counter);

      outputEditor.setReadOnly(true);
      outputEditor.getSession().setMode('ace/mode/aql');
      outputEditor.setOption('vScrollBarAlwaysVisible', true);
      outputEditor.setOption('showPrintMargin', false);
      this.setEditorAutoHeight(outputEditor);

      // Store sent query and bindParameter
      this.queriesHistory[counter] = {
        sentQuery: this.aqlEditor.getValue(),
        bindParam: this.bindParamTableObj
      };

      this.fillExplain(outputEditor, counter, profile);
      this.outputCounter++;
    },

    debugDownloadDialog: function () {
      var buttons = [];
      var tableContent = [];

      tableContent.push(
        window.modalView.createReadOnlyEntry(
          'debug-download-package-disclaimer',
          'Disclaimer',
          '<p>This will generate a package containing a lot of commonly required information about your query and environment that helps the ArangoDB Team to reproduce your issue. This debug package will include:</p>' +
            '<ul>' +
            '<li>collection names</li>' +
            '<li>collection indexes</li>' +
            '<li>attribute names</li>' +
            '<li>bind parameters</li>' +
            '</ul>' +
            '<p>Additionally, samples of your data will be included with all <b>string values obfuscated</b> in a non-reversible way if below checkbox is ticked.</p>' +
            '<p>If disabled, this package will not include any data.</p>' +
            '<p>Please open the package locally and check if it contains anything that you are not allowed/willing to share and obfuscate it before uploading. Including this package in bug reports will lower the amount of questioning back and forth to reproduce the issue on our side and is much appreciated.</p>',
          undefined,
          false,
          false
        )
      );
      tableContent.push(
        window.modalView.createCheckboxEntry(
          'debug-download-package-examples',
          'Include obfuscated examples',
          'includeExamples',
          'Includes an example set of documents, obfuscating all string values inside the data. This helps the ArangoDB Team as many issues are related to the document structure / format and the indexes defined on them.',
          true
        )
      );
      buttons.push(
        window.modalView.createSuccessButton('Download Package', this.downloadDebugZip.bind(this))
      );
      window.modalView.show('modalTable.ejs', 'Download Query Debug Package', buttons, tableContent, undefined, undefined);
    },

    downloadDebugZip: function () {
      if (this.verifyQueryAndParams()) {
        return;
      }

      var cbFunction = function () {
        window.modalView.hide();
      };
      var errorFunction = function (errorCode, response) {
        window.arangoHelper.arangoError('Debug Dump', errorCode + ': ' + response);
        window.modalView.hide();
      };

      var query = this.aqlEditor.getValue();
      if (query !== '' && query !== undefined && query !== null) {
        var url = 'query/debugDump';
        var body = {
          query: query,
          bindVars: this.bindParamTableObj || {},
          examples: $('#debug-download-package-examples').is(':checked')
        };
        arangoHelper.downloadPost(url, JSON.stringify(body), cbFunction, errorFunction);
      } else {
        arangoHelper.arangoError('Query error', 'Could not create a debug package.');
      }
    },

    fillExplain: function (outputEditor, counter, profile) {
      var self = this;
      var queryData;

      if (profile) {
        queryData = this.readQueryData(null, null, true);
      } else {
        queryData = this.readQueryData();
      }

      if (queryData) {
        $('#outputEditorWrapper' + counter + ' .queryExecutionTime').text('');
        this.execPending = false;

        var afterResult = function () {
          $('#outputEditorWrapper' + counter + ' #spinner').remove();
          $('#outputEditor' + counter).css('opacity', '1');
          $('#outputEditorWrapper' + counter + ' .fa-close').show();
          $('#outputEditorWrapper' + counter + ' .switchAce').show();
        };

        var url;
        if (profile) {
          url = arangoHelper.databaseUrl('/_admin/aardvark/query/profile');
        } else {
          url = arangoHelper.databaseUrl('/_admin/aardvark/query/explain');
        }

        $.ajax({
          type: 'POST',
          url: url,
          data: JSON.stringify(queryData),
          contentType: 'application/json',
          processData: false,
          success: function (data) {
            if (data.msg && data.msg.errorMessage) {
              self.removeOutputEditor(counter);
              if (profile) {
                arangoHelper.arangoError('Profile', data.msg);
              } else {
                arangoHelper.arangoError('Explain', data.msg);
              }
            } else {
              // cache explain results
              self.cachedQueries[counter] = data;

              outputEditor.setValue(data.msg, 1);
              self.deselect(outputEditor);
              Noty.clearQueue();
              Noty.closeAll();
              self.handleResult(counter);

              // SCROLL TO RESULT BOX
              $('.centralRow').animate({ scrollTop: $('#queryContent').height() }, 'fast');
            }
            afterResult();
          },
          error: function (data) {
            try {
              var temp = JSON.parse(data.responseText);
              if (profile) {
                arangoHelper.arangoError('Profile', temp.errorMessage);
              } else {
                arangoHelper.arangoError('Explain', temp.errorMessage);
              }
            } catch (e) {
              if (profile) {
                arangoHelper.arangoError('Profile', 'ERROR');
              } else {
                arangoHelper.arangoError('Explain', 'ERROR');
              }
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
      if (this.renderComplete === false && this.aqlEditor) {
        // get cached query if available
        var queryObject = this.getCachedQuery();
        var self = this;

        if (queryObject !== null && queryObject !== undefined && queryObject !== '' && Object.keys(queryObject).length > 0) {
          this.aqlEditor.setValue(queryObject.query, 1);

          var queryName = sessionStorage.getItem('lastOpenQuery');

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
              self.fillBindParamTable(self.bindParamTableObj);

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
        var cache = sessionStorage.getItem('cachedQuery');
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
      if (query !== '') {
        if (Storage !== 'undefined') {
          var myObject = {
            query: query,
            parameter: vars
          };
          this.currentQuery = myObject;
          sessionStorage.setItem('cachedQuery', JSON.stringify(myObject));
        }
      }
    },

    closeAceResults: function (counter, target) {
      var self = this;
      if (counter) {
        ace.edit('outputEditor' + counter).destroy();
      } else {
        ace.edit($(target).attr('id')).destroy();
      }
      $('#outputEditorWrapper' + this.outputCounter).hide();

      var cleanup = function (target) {
        $(target).hide('fast', function () {
          // remove dom
          $(target).remove();
          if ($('.outputEditorWrapper').length === 0) {
            self.cachedQueries = {};
            $('#removeResults').hide();
          }
        });
      };

      if (target) {
        cleanup(target);
      } else {
        _.each($('#outputEditors').children(), function (elem) {
          cleanup(elem);
        });
      }
    },

    closeResult: function (e) {
      var self = this;
      var target = $('#' + $(e.currentTarget).attr('element')).parent();
      var id = $(target).attr('id');
      var counter = id.replace(/^\D+/g, '');

      // remove unused ace editor instances
      self.closeAceResults(counter, target);

      delete this.cachedQueries[counter];
      delete this.queriesHistory[counter];
    },

    fillSelectBoxes: function () {
      // fill select box with # of results
      var querySize = 1000;
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

      if (this.bindParamMode === 'json') {
        this.toggleBindParams();
      }

      this.initDone = true;
      this.renderBindParamTable(true);
      this.restoreCachedQueries();
      this.delegateEvents();
      this.restoreQuerySize();
      this.getCachedQueryAfterRender();
    },

    cleanupGraphs: function () {
      if (this.graphViewers !== undefined || this.graphViewers !== null) {
        _.each(this.graphViewers, function (graphView) {
          if (graphView !== undefined) {
            graphView.killCurrentGraph();
            graphView.remove();
          }
        });
        $('canvas').remove();

        this.graphViewers = null;
        this.graphViewers = [];
      }
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

    restoreCachedQueries: function () {
      var self = this;

      if (Object.keys(this.cachedQueries).length > 0) {
        _.each(this.cachedQueries, function (query, counter) {
          self.renderQueryResultBox(counter, null, true);
          self.renderQueryResult(query, counter, true);

          if (query.sentQuery) {
            self.bindQueryResultButtons(null, counter);
          }
        });
        $('#removeResults').show();
      }
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
        } else if (self.bindParamTableObj[word] === null) {
          newObject[word] = null;
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

      this.checkForNewBindParams();
      _.each(this.bindParamTableObj, function (val, key) {
        $('#arangoBindParamTable tbody').append(
          '<tr>' +
          '<td>' + key + '</td>' +
          '<td><input name=' + key + ' type="text"></input></td>' +
          '</tr>'
        );
        counter++;
      });
      if (counter === 0) {
        $('#arangoBindParamTable tbody').append(
          '<tr class="noBgColor">' +
          '<td>No bind parameters defined.</td>' +
          '<td></td>' +
          '</tr>'
        );
      } else {
        this.fillBindParamTable(this.bindParamTableObj);
      }

      // check if existing entry already has a stored value
      var queryName = sessionStorage.getItem('lastOpenQuery');
      var query = this.collection.findWhere({name: queryName});

      try {
        query = query.toJSON();
      } catch (ignore) {
      }

      if (query && this.loadedCachedQuery === false) {
        this.loadedCachedQuery = true;
        this.fillBindParamTable(query.parameter);
      }
    },

    fillBindParamTable: function (object) {
      _.each(object, function (val, key) {
        _.each($('#arangoBindParamTable input'), function (element) {
          if ($(element).attr('name') === key) {
            if (val instanceof Array) {
              $(element).val(JSON.stringify(val)).addClass('arraytype');
            } else if (typeof val === 'object') {
              $(element).val(JSON.stringify(val)).addClass(typeof val + 'type');
            } else if (typeof val === 'number') {
              $(element).val(val).addClass(typeof val + 'type');
            } else {
              var isNumber = false;
              // check if a potential string value is a number
              try {
                if (typeof JSON.parse(val) === 'number') {
                  isNumber = true;
                }
              } catch (ignore) {
              }

              if (!isNumber) {
                $(element).val(val).addClass(typeof val + 'type');
              } else {
                $(element).val('"' + val + '"').addClass(typeof val + 'type');
              }
            }
          }
        });
      });
    },

    initAce: function () {
      var self = this;

      // init aql editor
      this.aqlEditor = ace.edit('aqlEditor');
      this.aqlEditor.$blockScrolling = Infinity;
      this.aqlEditor.getSession().setMode('ace/mode/aql');
      this.aqlEditor.getSession().setUseWrapMode(true);
      this.aqlEditor.setFontSize('10pt');
      this.aqlEditor.setShowPrintMargin(false);

      this.bindParamAceEditor = ace.edit('bindParamAceEditor');
      this.bindParamAceEditor.$blockScrolling = Infinity;
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
        // case copy-paste: query completely selected & removed
        if (self.aqlEditor.getValue().length < 1) {
          if (Object.keys(self.bindParamTableObj).length > 0) {
            // query is empty but bindvars are defined
            self.lastCachedBindParameter = self.bindParamTableObj;
          }
        }

        self.checkForNewBindParams();
        self.renderBindParamTable();

        if (self.parseQuery(self.aqlEditor.getValue()).bindParams.length > 0) {
          var restoreAttr = [];
          _.each(self.parseQuery(self.aqlEditor.getValue()).bindParams, function (name) {
            if ($('input[name=\'' + name + '\']') !== undefined && $('input[name=\'' + name + '\']').length > 0) {
              if ($('input[name=\'' + name + '\']').val().length === 0) {
                if (self.lastCachedBindParameter) {
                  var value = $('input[name=\'' + name + '\']').val();
                  if (self.lastCachedBindParameter[name]) {
                    if (self.lastCachedBindParameter[name] !== value) {
                      restoreAttr.push(name);
                    }
                  }
                }
              }
            }
          });
          if (restoreAttr.length > 0) {
            // self.bindParamTableObj = self.lastCachedBindParameter;
            var toRestore = {};
            _.each(restoreAttr, function (name, val) {
              toRestore[name] = self.lastCachedBindParameter[name];
            });
            self.bindParamTableObj = toRestore;
            self.renderBindParamTable();
          }
        }

        if (self.initDone) {
          self.setCachedQuery(self.aqlEditor.getValue(), JSON.stringify(self.bindParamTableObj));
        }

        self.bindParamAceEditor.setValue(JSON.stringify(self.bindParamTableObj, null, '\t'), 1);
        $('#aqlEditor .ace_text-input').focus();

        self.resize();
      });

      var setOutputEditorFontSize = function (size) {
        _.each($('.outputEditors'), function (value) {
          var id = $(value).children().first().attr('id');
          id = id.replace('Wrapper', '');
          var outputEditor = ace.edit(id);
          outputEditor.setFontSize(size);
        });
      };

      var editors = [this.aqlEditor, this.bindParamAceEditor];
      _.each(editors, function (editor) {
        editor.commands.addCommand({
          name: 'togglecomment',
          bindKey: {win: 'Ctrl-Shift-C', linux: 'Ctrl-Shift-C', mac: 'Command-Shift-C'},
          exec: function (editor) {
            editor.toggleCommentLines();
          },
          multiSelectAction: 'forEach'
        });

        editor.commands.addCommand({
          name: 'increaseFontSize',
          bindKey: {win: 'Shift-Alt-Up', linux: 'Shift-Alt-Up', mac: 'Shift-Alt-Up'},
          exec: function (editor) {
            var newSize = parseInt(self.aqlEditor.getFontSize().match(/\d+/)[0], 10) + 1;
            newSize += 'pt';
            self.aqlEditor.setFontSize(newSize);
            setOutputEditorFontSize(newSize);
          },
          multiSelectAction: 'forEach'
        });

        editor.commands.addCommand({
          name: 'decreaseFontSize',
          bindKey: {win: 'Shift-Alt-Down', linux: 'Shift-Alt-Down', mac: 'Shift-Alt-Down'},
          exec: function (editor) {
            var newSize = parseInt(self.aqlEditor.getFontSize().match(/\d+/)[0], 10) - 1;
            newSize += 'pt';
            self.aqlEditor.setFontSize(newSize);
            setOutputEditorFontSize(newSize);
          },
          multiSelectAction: 'forEach'
        });

        editor.commands.addCommand({
          name: 'executeQuery',
          bindKey: {win: 'Ctrl-Return', mac: 'Command-Return', linux: 'Ctrl-Return'},
          exec: function () {
            self.executeQuery();
          }
        });

        editor.commands.addCommand({
          name: 'executeSelectedQuery',
          bindKey: {win: 'Ctrl-Alt-Return', mac: 'Command-Alt-Return', linux: 'Ctrl-Alt-Return'},
          exec: function () {
            self.executeQuery(undefined, true);
          }
        });

        editor.commands.addCommand({
          name: 'saveQuery',
          bindKey: {win: 'Ctrl-Shift-S', mac: 'Command-Shift-S', linux: 'Ctrl-Shift-S'},
          exec: function () {
            self.addAQL();
          }
        });

        editor.commands.addCommand({
          name: 'explainQuery',
          bindKey: {win: 'Ctrl-Shift-Return', mac: 'Command-Shift-Return', linux: 'Ctrl-Shift-Return'},
          exec: function () {
            self.explainQuery();
          }
        });

        editor.commands.addCommand({
          name: 'togglecomment',
          bindKey: {win: 'Ctrl-Shift-C', linux: 'Ctrl-Shift-C', mac: 'Command-Shift-C'},
          exec: function (editor) {
            editor.toggleCommentLines();
          },
          multiSelectAction: 'forEach'
        });

        editor.commands.addCommand({
          name: 'showSpotlight',
          bindKey: {win: 'Ctrl-Space', mac: 'Ctrl-Space', linux: 'Ctrl-Space'},
          exec: function () {
            self.showSpotlight();
          }
        });
      });

      this.queryPreview = ace.edit('queryPreview');
      this.queryPreview.getSession().setMode('ace/mode/aql');
      this.queryPreview.setReadOnly(true);
      this.queryPreview.setFontSize('13px');
      this.queryPreview.setShowPrintMargin(false);

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
      var queryName = $('#lastQueryName').html();
      var query = this.collection.findWhere({name: queryName});

      if (query) {
        // SET QUERY STRING
        query.set('value', content);
        // SET QUERY BIND PARAMS
        query.set('parameter', this.bindParamTableObj);

        var callback = function (error) {
          if (error) {
            arangoHelper.arangoError('Query', 'Could not save query');
          } else {
            var self = this;
            arangoHelper.arangoNotification('Saved query', '"' + queryName + '"');
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
      sessionStorage.setItem('lastOpenQuery', undefined);
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
              rule: Joi.string().regex(/^(\s*[a-zA-Z0-9\-._]+\s*)+$/).required(),
              msg: 'A query name is required. Characters, numbers and ".", "_", "-" symbols are allowed.'
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
      var saveName = arangoHelper.escapeHtml($('#new-query-name').val());
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

      var saveName = arangoHelper.escapeHtml($('#new-query-name').val());
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
            'Query: <span id="lastQueryName">' + name + '</span>'
          );
        } else {
          $('#subNavigationBar .breadcrumb').html('');
        }
      }, 50);
    },

    verifyQueryAndParams: function () {
      var quit = false;
      var self = this;

      if (this.aqlEditor.getValue().length === 0) {
        arangoHelper.arangoError('Query', 'Your query is empty');
        quit = true;
      }

      // if query bind parameter editor is visible
      if ($('#bindParamAceEditor').is(':visible')) {
        try {
          // check if parameters are parseable
          JSON.parse(self.bindParamAceEditor.getValue());
        } catch (e) {
          arangoHelper.arangoError('Bind Parameter', 'Could not parse bind parameter');
          quit = true;
        }
      }

      return quit;
    },

    executeQuery: function (e, selected) {
      if (this.verifyQueryAndParams()) {
        return;
      }

      $('#outputEditorWrapper' + this.outputCounter).hide();
      $('#outputEditorWrapper' + this.outputCounter).show('fast');

      this.lastSentQueryString = this.aqlEditor.getValue();

      this.renderQueryResultBox(this.outputCounter, selected);
    },

    renderQueryResultBox: function (counter, selected, cached) {
      this.$(this.outputDiv).prepend(this.outputTemplate.render({
        counter: counter,
        type: 'Query'
      }));

      var outputEditor = ace.edit('outputEditor' + counter);

      // store query and bind parameters history

      outputEditor.setFontSize('13px');
      outputEditor.$blockScrolling = Infinity;
      outputEditor.getSession().setMode('ace/mode/json');
      outputEditor.setReadOnly(true);
      outputEditor.setOption('vScrollBarAlwaysVisible', true);
      outputEditor.setShowPrintMargin(false);
      this.setEditorAutoHeight(outputEditor);

      if (!cached) {
        // Store sent query and bindParameter
        this.queriesHistory[counter] = {
          sentQuery: this.aqlEditor.getValue(),
          bindParam: this.bindParamTableObj
        };

        this.fillResult(counter, selected);
        this.outputCounter++;
      }
    },

    readQueryData: function (selected, forExecute, forProfile) {
      var data = {};

      if (!forProfile) {
        data.id = 'currentFrontendQuery';
      }

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
        return false;
      } else {
        var bindVars = {};
        if (Object.keys(this.bindParamTableObj).length > 0) {
          _.each(this.bindParamTableObj, function (val, key) {
            if (data.query.indexOf(key) > -1) {
              bindVars[key] = val;
            }
          });
          data.bindVars = this.bindParamTableObj;
        }
        if (Object.keys(bindVars).length > 0) {
          data.bindVars = bindVars;
        }

        // add profile flag for query execution
        if (forExecute) {
          data.options = {
            profile: true
          };
        }
      }

      return data;
    },

    fillResult: function (counter, selected) {
      var self = this;

      var queryData = this.readQueryData(selected, true);

      if (queryData) {
        $.ajax({
          type: 'POST',
          url: arangoHelper.databaseUrl('/_api/cursor'),
          headers: {
            'x-arango-async': 'store'
          },
          data: JSON.stringify(queryData),
          contentType: 'application/json',
          processData: false,
          success: function (data, textStatus, xhr) {
            if (xhr.getResponseHeader('x-arango-async-id')) {
              self.queryCallbackFunction(xhr.getResponseHeader('x-arango-async-id'), counter);
            }
            Noty.clearQueue();
            Noty.closeAll();
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

    warningsFunc: function (data, outputEditor) {
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
    },

    renderQueryResult: function (data, counter, cached, queryID) {
      var self = this;
      var result;

      var activeSubView = 'query';
      try {
        activeSubView = window.App.naviView.activeSubMenu.route;
      } catch (ignore) {
      }

      if (window.location.hash === '#queries' && activeSubView === 'query') {
        var outputEditor = ace.edit('outputEditor' + counter);

        var maxHeight = $('.centralRow').height() - 250;
        var success;
        var invalidGeoJSON = 0;

        // handle explain query case
        if (!data.msg) {
          // handle usual query
          result = self.analyseQuery(data.result);
          if (result.defaultType === 'table' || result.defaultType === 'geotable') {
            $('#outputEditorWrapper' + counter + ' .arangoToolbarTop').after(
              '<div id="outputTable' + counter + '" class="outputTable"></div>'
            );
            $('#outputTable' + counter).show();
            self.renderOutputTable(result, counter);

            // apply max height for table output dynamically
            $('.outputEditorWrapper .tableWrapper').css('max-height', maxHeight);

            $('#outputEditor' + counter).hide();
            success = true;
          } else if (result.defaultType === 'graph') {
            $('#outputEditorWrapper' + counter + ' .arangoToolbarTop').after('<div id="outputGraph' + counter + '"></div>');
            $('#outputGraph' + counter).show();
            success = self.renderOutputGraph(result, counter);

            if (success) {
              $('#outputEditor' + counter).hide();

              $('#outputEditorWrapper' + counter + ' #copy2gV').show();
              $('#outputEditorWrapper' + counter + ' #copy2gV').bind('click', function () {
                self.showResultInGraphViewer(result, counter);
              });
            } else {
              $('#outputGraph' + counter).remove();
            }
          }
          if (result.defaultType === 'geo' || result.defaultType === 'geotable') {
            var geoHeight = 500;
            $('#outputEditor' + counter).hide();
            $('#outputEditorWrapper' + counter + ' .arangoToolbarTop').after(
              '<div class="geoContainer" id="outputGeo' + counter + '" style="height: ' + geoHeight + 'px; maxheight: ' + maxHeight + 'px;"></div>'
            );

            self.maps[counter] = L.map('outputGeo' + counter).setView([51.505, -0.09], 13);

            self.maps[counter].addLayer(new L.StamenTileLayer('terrain', {
              detectRetina: true
            }));

            var position = 1;
            var geojson;

            var geoStyle = {
              color: '#3498db',
              opacity: 1,
              weight: 3
            };

            var geojsonMarkerOptions = {
              radius: 8,
              fillColor: '#2ecc71',
              color: 'white',
              weight: 1,
              opacity: 1,
              fillOpacity: 0.64
            };

            var markers = [];
            _.each(data.result, function (geo) {
              var geometry = {};
              if (geo.hasOwnProperty('geometry')) {
                geometry = geo.geometry;
              } else {
                geometry = geo;
              }

              if (geometry.type === 'Point' || geometry.type === 'MultiPoint') {
                // reverse neccessary if we are using GeoJSON order
                // L.marker(geo.coordinates.reverse()).addTo(self.maps[counter]);
                try {
                  geojson = new L.GeoJSON(geometry, {
                    onEachFeature: function (feature, layer, x) {
                      layer.bindPopup('<pre style="width: 250px; max-height: 250px;">' + JSON.stringify(geo, null, 2) + '</pre>');
                    },
                    pointToLayer: function (feature, latlng) {
                      var res = L.circleMarker(latlng, geojsonMarkerOptions);
                      markers.push(res);
                      return res;
                    }
                  }).addTo(self.maps[counter]);
                } catch (ignore) {
                  invalidGeoJSON++;
                }
              } else if (geometry.type === 'Polygon' || geometry.type === 'LineString' || geometry.type === 'MultiLineString' || geometry.type === 'MultiPolygon') {
                try {
                  geojson = new L.GeoJSON(geometry, {
                    style: geoStyle,
                    onEachFeature: function (feature, layer) {
                      layer.bindPopup('<pre style="width: 250px;">' + JSON.stringify(feature, null, 2) + '</pre>');
                    }
                  }).addTo(self.maps[counter]);
                  markers.push(geojson);
                } catch (ignore) {
                  invalidGeoJSON++;
                }
              }

              // positioning
              if (position === data.result.length) {
                if (markers.length > 0) {
                  try {
                    var show = new L.FeatureGroup(markers);
                    self.maps[counter].fitBounds(show.getBounds());
                  } catch (ignore) {
                  }
                } else {
                  try {
                    self.maps[counter].fitBounds(geojson.getBounds());
                  } catch (ignore) {
                  }
                }
              }
              position++;
            });
          }

          // add active class to choosen display method
          if (success !== false) {
            if (result.defaultType === 'geotable' || result.defaultType === 'geo') {
              $('#outputTable' + counter).hide();
              if (result.defaultType === 'geotable') {
                $('#table-switch').css('display', 'inline');
              }

              if (data.result.length === invalidGeoJSON) {
                $('#outputGeo' + counter).remove();
                if (result.defaultType === 'geotable') {
                  $('#outputTable' + counter).show();
                  $('#table-switch').addClass('active').css('display', 'inline');
                } else {
                  $('#outputEditor' + counter).show();
                  $('#json-switch').addClass('active').css('display', 'inline');
                }
              } else {
                $('#geo-switch').addClass('active').css('display', 'inline');
              }
            } else {
              $('#' + result.defaultType + '-switch').addClass('active').css('display', 'inline');
            }
            $('#json-switch').css('display', 'inline');

            // fallback
            if (result.fallback && (result.fallback === 'geo' || result.fallback === 'geotable')) {
              $('#geo-switch').addClass('disabled').css('display', 'inline').css('opacity', '0.5');
              $('#geo-switch').addClass('tippy').attr('title', 'No internet connection. Map is not available.');
              arangoHelper.createTooltips();
            }
          }

          var appendSpan = function (value, icon, css) {
            if (!css) {
              css = '';
            }
            $('#outputEditorWrapper' + counter + ' .arangoToolbarTop .pull-left').append(
              '<span class="' + css + '"><i class="fa ' + icon + '"></i><i class="iconText">' + value + '</i></span>'
            );
          };

          var time = '-';
          if (data && data.extra && data.extra.stats) {
            if (data.extra.stats.executionTime > 1) {
              time = numeral(data.extra.stats.executionTime).format('0.000');
              time += ' s';
            } else {
              time = numeral(data.extra.stats.executionTime * 1000).format('0.000');
              time += ' ms';
            }
          }
          appendSpan(
            data.result.length + ' elements', 'fa-calculator'
          );
          appendSpan(time, 'fa-clock-o');

          if (data.extra) {
            if (data.extra.profile) {
              appendSpan('', 'fa-caret-down');
              self.appendProfileDetails(counter, data.extra.profile);
            }

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
        }

        $('#outputEditorWrapper' + counter + ' .pull-left #spinner').remove();
        $('#outputEditorWrapper' + counter + ' #cancelCurrentQuery').remove();

        self.warningsFunc(data, outputEditor);
        window.progressView.hide();

        $('#outputEditorWrapper' + counter + ' .switchAce').show();
        $('#outputEditorWrapper' + counter + ' .fa-close').show();
        $('#outputEditor' + counter).css('opacity', '1');

        if (!data.msg) {
          $('#outputEditorWrapper' + counter + ' #downloadQueryResult').show();
          $('#outputEditorWrapper' + counter + ' #copy2aqlEditor').show();
        }

        self.setEditorAutoHeight(outputEditor);
        self.deselect(outputEditor);

        // when finished send a delete req to api (free db space)
        // deletion only necessary if result was not fully fetched
        var url;
        if (queryID && data.hasMore) {
          url = arangoHelper.databaseUrl('/_api/cursor/' + encodeURIComponent(queryID));
        } else {
          if (data.id && data.hasMore) {
            url = arangoHelper.databaseUrl('/_api/cursor/' + encodeURIComponent(data.id));
          }
        }

        if (url) {
          $.ajax({
            url: url,
            type: 'DELETE'
          });
        }

        if (!cached) {
          // cache the query
          self.cachedQueries[counter] = data;

          // cache the original sent aql string
          this.cachedQueries[counter].sentQuery = self.aqlEditor.getValue();
        }

        if (data.msg) {
          if (data.extra.profile) {
            $('#outputEditorWrapper' + counter + ' .toolbarType').html('Profile');
          } else {
            $('#outputEditorWrapper' + counter + ' .toolbarType').html('Explain');
          }
          outputEditor.setValue(data.msg, 1);
        }

        if (result.defaultType === 'table' || result.defaultType === 'geotable') {
          // show csv download button
          self.checkCSV(counter);
        }
      } else {
        // if result comes in when view is not active
        // store the data into cachedQueries obj
        self.cachedQueries[counter] = data;
        self.cachedQueries[counter].sentQuery = self.lastSentQueryString;

        arangoHelper.arangoNotification('Query finished', 'Return to queries view to see the result.');
      }
    },

    bindQueryResultButtons: function (queryID, counter) {
      var self = this;

      if (queryID) {
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
      }

      $('#outputEditorWrapper' + counter + ' #cancelCurrentQuery').bind('click', function () {
        cancelRunningQuery(queryID, counter);
      });

      $('#outputEditorWrapper' + counter + ' #copy2aqlEditor').bind('click', function () {
        if (!$('#toggleQueries1').is(':visible')) {
          self.toggleQueries();
        }

        var aql = self.queriesHistory[counter].sentQuery;
        var bindParam = self.queriesHistory[counter].bindParam;

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
    },

    queryCallbackFunction: function (queryID, counter) {
      var self = this;
      self.tmpQueryResult = null;

      this.bindQueryResultButtons(queryID, counter);
      this.execPending = false;

      var userLimit;
      try {
        userLimit = parseInt($('#querySize').val());
      } catch (e) {
        arangoHelper.arangoError('Parse Error', 'Could not parse defined user limit.');
      }
      if (isNaN(userLimit)) {
        userLimit = true;
      }

      var pushQueryResults = function (data) {
        if (self.tmpQueryResult === null) {
          self.tmpQueryResult = {
            result: [],
            complete: true
          };
        }

        _.each(data, function (val, key) {
          if (key !== 'result') {
            self.tmpQueryResult[key] = val;
          } else {
            _.each(data.result, function (d) {
              if (self.tmpQueryResult.result.length <= userLimit || userLimit === true) {
                if (self.tmpQueryResult.result.length < userLimit || userLimit === true) {
                  self.tmpQueryResult.result.push(d);
                }
              } else {
                self.tmpQueryResult.complete = false;
              }
            });
          }
        });
      };

      // check if async query is finished
      var checkQueryStatus = function (cursorID) {
        var url = arangoHelper.databaseUrl('/_api/job/' + encodeURIComponent(queryID));
        if (cursorID) {
          url = arangoHelper.databaseUrl('/_api/cursor/' + encodeURIComponent(cursorID));
        }

        $.ajax({
          type: 'PUT',
          url: url,
          contentType: 'application/json',
          processData: false,
          success: function (data, textStatus, xhr) {
            // query finished, now fetch results using cursor
            var flag = true;
            if (self.tmpQueryResult && self.tmpQueryResult.result && self.tmpQueryResult.result.length) {
              if (self.tmpQueryResult.result.length < userLimit || userLimit === true) {
                flag = true;
              } else {
                flag = false;
              }
            }

            if (xhr.status === 201 || xhr.status === 200) {
              if (data.hasMore && flag) {
                pushQueryResults(data);

                // continue to fetch result
                checkQueryStatus(data.id);
              } else {
                // finished fetching
                pushQueryResults(data);
                self.renderQueryResult(self.tmpQueryResult, counter, false, queryID);
                self.tmpQueryResult = null;
                // SCROLL TO RESULT BOX
                $('.centralRow').animate({ scrollTop: $('#queryContent').height() }, 'fast');
              }
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
              if (error.code !== 400 && error.code !== 404 && error.code !== 500 && error.code !== 403 && error.code !== 501) {
                arangoHelper.arangoNotification('Query', 'Successfully aborted.');
              }
            }

            window.progressView.hide();
          }
        });
      };
      checkQueryStatus();
    },

    appendProfileDetails: function (counter, data) {
      var element = '#outputEditorWrapper' + counter;

      $(element + ' .fa-caret-down').first().on('click', function () {
        var alreadyRendered = $(element).find('.queryProfile');
        if (!$(alreadyRendered).is(':visible')) {
          $(element).append('<div class="queryProfile" counter="' + counter + '"></div>');
          var queryProfile = $(element + ' .queryProfile').first();
          queryProfile.hide();

          // var outputPosition = $(element + ' .fa-caret-down').first().offset();
          queryProfile
            .css('position', 'absolute')
            .css('left', 215)
            .css('top', 55);

          // $("#el").offset().top - $(document).scrollTop()
          var profileWidth = 590;

          var legend = [
            'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'
          ];

          var colors = [
            'rgb(48, 125, 153)',
            'rgb(241, 124, 176)',
            'rgb(137, 110, 37)',
            'rgb(93, 165, 218)',
            'rgb(250, 164, 58)',
            'rgb(64, 74, 83)',
            'rgb(96, 189, 104)',
            'rgb(221, 224, 114)'
          ];

          var descs = [
            'startup time for query engine',
            'query parsing',
            'abstract syntax tree optimizations',
            'loading collections',
            'instanciation of initial execution plan',
            'execution plan optimization and permutation',
            'query execution',
            'query finalization'
          ];

          queryProfile.append(
            '<i class="fa fa-close closeProfile"></i>' +
            '<span class="profileHeader">Profiling information</span>' +
            '<div class="pure-g pure-table pure-table-body" style="width: auto;"></div>' +
            '<div class="prof-progress"></div>' +
            '<div class="prof-progress-label"></div>' +
            '<div class="clear"></div>'
          );

          var total = 0;
          _.each(data, function (value, key) {
            if (key !== 'finished') {
              total += value * 1000;
            }
          });

          var pos = 0;
          var width;
          var adjustWidth = 0;

          var time = '';
          _.each(data, function (value, key) {
            if (key !== 'finished') {
              if (value > 1) {
                time = numeral(value).format('0.000');
                time += ' s';
              } else {
                time = numeral(value * 1000).format('0.000');
                time += ' ms';
              }

              queryProfile.find('.pure-g').append(
                '<div class="pure-table-row noHover">' +
                '<div class="pure-u-1-24 left"><p class="bold" style="background:' + colors[pos] + '">' + legend[pos] + '</p></div>' +
                '<div class="pure-u-4-24 left">' + time + '</div>' +
                '<div class="pure-u-6-24 left">' + key + '</div>' +
                '<div class="pure-u-13-24 left">' + descs[pos] + '</div>' +
                '</div>'
              );

              width = Math.floor((value * 1000) / total * 100);
              if (width === 0) {
                width = 1;
                adjustWidth++;
              }

              if (pos !== 7) {
                queryProfile.find('.prof-progress').append(
                  '<div style="width: ' + width + '%; background-color: ' + colors[pos] + '"></div>'
                );
                if (width > 1) {
                  queryProfile.find('.prof-progress-label').append(
                    '<div style="width: ' + width + '%;">' + legend[pos] + '</div>'
                  );
                } else {
                  queryProfile.find('.prof-progress-label').append(
                    '<div style="width: ' + width + '%; font-size: 9px">' + legend[pos] + '</div>'
                  );
                }
              } else {
                if (adjustWidth > 0) {
                  width = width - adjustWidth;
                }
                queryProfile.find('.prof-progress').append(
                  '<div style="width: ' + width + '%; background-color: ' + colors[pos] + '"></div>'
                );
                if (width > 1) {
                  queryProfile.find('.prof-progress-label').append(
                    '<div style="width: ' + width + '%;">' + legend[pos] + '</div>'
                  );
                } else {
                  queryProfile.find('.prof-progress-label').append(
                    '<div style="width: ' + width + '%; font-size: 9px">' + legend[pos] + '</div>'
                  );
                }
              }
              pos++;
            }
          });

          queryProfile.width(profileWidth);
          queryProfile.height('auto');
          queryProfile.fadeIn('fast');
        } else {
          $(element).find('.queryProfile').remove();
        }
      });
    },

    analyseQuery: function (result) {
      var toReturn = {
        defaultType: null,
        original: result,
        modified: null
      };

      var found = false;

      if (!Array.isArray(result)) {
        toReturn.defaultType = 'json';
        return toReturn;
      }

      // check if result could be displayed as graph
      // case a) result has keys named vertices and edges

      var index = 0;
      for (var i = 0; i < result.length; i++) {
        if (result[i]) {
          index = i;
          break;
        }
      }

      if (result[index]) {
        if (result[index].vertices && result[index].edges) {
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

          var percentagea = 0;
          if (totala > 0) {
            percentagea = hitsa / totala * 100;
          }

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
            if (obj) {
              if (obj._from && obj._to && obj._id) {
                hitsb++;
              }
            }
          });

          var percentageb = 0;
          if (totalb > 0) {
            percentageb = hitsb / totalb * 100;
          }

          if (percentageb >= 95) {
            found = true;
            toReturn.defaultType = 'graph';
            toReturn.graphInfo = 'array';
            // then display as graph
          }
        }
      }

      // check if result could be displayed as table
      var geojson = 0;
      if (!found) {
        var check = true;
        var attributes = {};

        if (result.length < 1) {
          check = false;
        }

        if (check) {
          _.each(result, function (obj) {
            if (typeof obj !== 'object' || obj === null || Array.isArray(obj)) {
              // not a document and not suitable for tabluar display
              return;
            }

            if (typeof obj === 'object') {
              if (obj.hasOwnProperty('coordinates') && obj.hasOwnProperty('type')) {
                if (obj.type === 'Point' || obj.type === 'MultiPoint' ||
                  obj.type === 'Polygon' || obj.type === 'MultiPolygon' ||
                  obj.type === 'LineString' || obj.type === 'MultiLineString') {
                  geojson++;
                }
              } else if (obj.hasOwnProperty('geometry')) {
                try {
                  if (obj.geometry.hasOwnProperty('coordinates') && obj.geometry.hasOwnProperty('type')) {
                    if (obj.geometry.type === 'Point' || obj.geometry.type === 'MultiPoint' ||
                      obj.geometry.type === 'Polygon' || obj.geometry.type === 'MultiPolygon' ||
                      obj.geometry.type === 'LineString' || obj.geometry.type === 'MultiLineString') {
                      geojson++;
                    }
                  }
                } catch (err) {
                 // happens e.g. if doc.geomotry === null
                }
              }
            }

            _.each(obj, function (value, key) {
              if (attributes.hasOwnProperty(key)) {
                ++attributes[key];
              } else {
                attributes[key] = 1;
              }
            });
          });

          var rate = 0;

          _.each(attributes, function (val, key) {
            if (check !== false) {
              rate = (val / result.length) * 100;
              if (rate <= 95) {
                check = false;
              }
            }
          });

          if (rate <= 95) {
            check = false;
          }
        }

        if (check) {
          found = true;
          if (result.length === geojson) {
            toReturn.defaultType = 'geotable';
          } else {
            toReturn.defaultType = 'table';
          }
        }
      }

      if (!found) {
      // if all check fails, then just display as json
        if (result.length !== 0 && result.length === geojson) {
          toReturn.defaultType = 'geo';
        } else {
          toReturn.defaultType = 'json';
        }
      }

      if (toReturn.defaultType === 'geo' || toReturn.defaultType === 'geotable') {
        if (!window.activeInternetConnection) {
          // mark the type we wanted to render
          toReturn.fallback = toReturn.defaultType;

          if (toReturn.defaultType === 'geo') {
            toReturn.defaultType = 'json';
          } else {
            toReturn.defaultType = 'table';
          }
        }
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
        try {
          row = parseInt(row);
          if (row > 0) {
            row = row - 1;
          }
        } catch (ignore) {
        }

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
          arangoHelper.arangoError('Query', 'Could not reload queries');
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
      if (callback) {
        callback(false);
      }
      self.queries = window.aqltemplates;
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
      var headers = {}; // quick lookup cache
      var pos = 0;
      _.each(data.original, function (obj) {
        if (first === true && obj) {
          tableDescription.titles = Object.keys(obj);
          tableDescription.titles.forEach(function (t) {
            headers[String(t)] = pos++;
          });
          first = false;
        }
        var part = Array(pos);
        _.each(obj, function (val, key) {
          if (!headers.hasOwnProperty(key)) {
            // different attribute
            return;
          }

          if (typeof val === 'object') {
            val = JSON.stringify(val);
          }

          part[headers[key]] = val;
        });

        tableDescription.rows.push(part);
      });

      $('#outputTable' + counter).append(this.table.render({content: tableDescription}));
    },

    renderOutputGraph: function (data, counter) {
      this.graphViewers[counter] = new window.GraphViewer({
        name: undefined,
        documentStore: window.App.arangoDocumentStore,
        collection: new window.GraphCollection(),
        userConfig: window.App.userConfig,
        id: '#outputGraph' + counter,
        data: data
      });
      var success = this.graphViewers[counter].renderAQLPreview();

      return success;
    },

    showResultInGraphViewer: function (data, counter) {
      window.location.hash = '#aql_graph';

      // TODO better manage mechanism for both gv's
      if (window.App.graphViewer) {
        if (window.App.graphViewer.graphSettingsView) {
          window.App.graphViewer.graphSettingsView.remove();
        }
        window.App.graphViewer.remove();
      }

      window.App.graphViewer = new window.GraphViewer({
        name: undefined,
        documentStore: window.App.arangoDocumentStore,
        collection: new window.GraphCollection(),
        userConfig: window.App.userConfig,
        noDefinedGraph: true,
        data: data
      });
      window.App.graphViewer.renderAQL();
    },

    getAQL: function (originCallback) {
      var self = this;

      this.collection.fetch({
        success: function () {
          self.getCachedQueryAfterRender();

          // old storage method
          var item = sessionStorage.getItem('customQueries');
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
                sessionStorage.removeItem('customQueries');
              }
            };
            self.collection.saveCollectionQueries(callback);
          }
          self.updateLocalQueries();

          if (originCallback) {
            originCallback();
          }
        },
        error: function (data, resp) {
          arangoHelper.arangoError('User Queries', resp.responseText);
        }
      });
    },

    checkCSV: function (counter) {
      var outputEditor = ace.edit('outputEditor' + counter);
      var val = outputEditor.getValue();
      var status = false;

      var tmp;
      // method: do not parse nested values
      try {
        val = JSON.parse(val);
        status = true;
        _.each(val, function (row, key1) {
          _.each(row, function (entry, key2) {
            // if nested array or object found, do not offer csv download
            try {
              tmp = JSON.parse(entry);
              // if parse success -> arr or obj found
              if (typeof tmp === 'object') {
                status = false;
              }
            } catch (ignore) {
            }
          });
        });
      } catch (ignore) {
      }

      if (status) {
        $('#outputEditorWrapper' + counter + ' #downloadCsvResult').show();
      }
    },

    doCSV: function (json) {
      var inArray = this.arrayFrom(json);
      var outArray = [];
      for (var row in inArray) {
        outArray[outArray.length] = this.parse_object(inArray[row]);
      }

      return $.csv.fromObjects(outArray);
    },

    downloadCsvResult: function (e) {
      var counter = $(e.currentTarget).attr('counter');

      var csv;
      var outputEditor = ace.edit('outputEditor' + counter);
      var val = outputEditor.getValue();
      val = JSON.parse(val);

      csv = $.csv.fromObjects(val, {
        justArrays: true
      });

      if (csv.length > 0) {
        arangoHelper.downloadLocalBlob(csv, 'csv');
      } else {
        arangoHelper.arangoError('Query error', 'Could not download the result.');
      }
    }

  });
}());
