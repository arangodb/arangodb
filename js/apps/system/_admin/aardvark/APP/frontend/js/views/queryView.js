/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, EJS, $, setTimeout, localStorage, ace, Storage, window, _, console, btoa*/
/*global _, arangoHelper, templateEngine, jQuery, Joi, d3*/

(function () {
  "use strict";
  window.queryView = Backbone.View.extend({
    el: '#content',
    id: '#customsDiv',
    warningTemplate: templateEngine.createTemplate("warningList.ejs"),
    tabArray: [],
    execPending: false,

    initialize: function () {
      this.refreshAQL();
      this.tableDescription.rows = this.customQueries;
    },

    events: {
      "click #result-switch": "switchTab",
      "click #query-switch": "switchTab",
      'click #customs-switch': "switchTab",
      'click #submitQueryButton': 'submitQuery',
      'click #explainQueryButton': 'explainQuery',
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
      'mouseover #querySelect': function(){this.refreshAQL(true);},
      'change #querySelect': 'importSelected',
      'keypress #aqlEditor': 'aqlShortcuts',
      'click #arangoQueryTable .table-cell0': 'editCustomQuery',
      'click #arangoQueryTable .table-cell1': 'editCustomQuery',
      'click #arangoQueryTable .table-cell2 a': 'deleteAQL',
      'click #confirmQueryImport': 'importCustomQueries',
      'click #confirmQueryExport': 'exportCustomQueries',
      'click #export-query': 'exportCustomQueries',
      'click #import-query': 'openExportDialog',
      'click #closeQueryModal': 'closeExportDialog',
      'click #downloadQueryResult': 'downloadQueryResult'
    },

    openExportDialog: function() {
      $('#queryImportDialog').modal('show'); 
    },

    closeExportDialog: function() {
      $('#queryImportDialog').modal('hide'); 
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

    updateTable: function () {
      this.tableDescription.rows = this.customQueries;

      _.each(this.tableDescription.rows, function(k) {
        k.thirdRow = '<a class="deleteButton"><span class="icon_arangodb_roundminus"' +
                     ' title="Delete query"></span></a>';
        if (k.hasOwnProperty('parameter')) {
          delete k.parameter;
        }
      });

      // escape all columns but the third (which contains HTML)
      this.tableDescription.unescaped = [ false, false, true ];

      this.$(this.id).html(this.table.render({content: this.tableDescription}));
    },

    editCustomQuery: function(e) {
      var queryName = $(e.target).parent().children().first().text(),
      inputEditor = ace.edit("aqlEditor"),
      varsEditor = ace.edit("varsEditor");
      inputEditor.setValue(this.getCustomQueryValueByName(queryName));
      varsEditor.setValue(JSON.stringify(this.getCustomQueryParameterByName(queryName)));
      this.deselect(varsEditor);
      this.deselect(inputEditor);
      $('#querySelect').val(queryName);
      this.switchTab("query-switch");
    },

    initTabArray: function() {
      var self = this;
      $(".arango-tab").children().each( function() {
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
      var found = this.customQueries.some(function(query){
        return query.name === saveName;
      });
      if(found){
        $('#modalButton1').removeClass('button-success');
        $('#modalButton1').addClass('button-warning');
        $('#modalButton1').text('Update');
      } else {
        $('#modalButton1').removeClass('button-warning');
        $('#modalButton1').addClass('button-success');
        $('#modalButton1').text('Save');
      }
    },

    clearOutput: function () {
      var outputEditor = ace.edit("queryOutput");
      outputEditor.setValue('');
    },

    clearInput: function () {
      var inputEditor = ace.edit("aqlEditor"),
      varsEditor = ace.edit("varsEditor");
      this.setCachedQuery(inputEditor.getValue(), varsEditor.getValue());
      inputEditor.setValue('');
      varsEditor.setValue('');
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
      var self = this;
      this.$el.html(this.template.render({}));
      this.$(this.id).html(this.table.render({content: this.tableDescription}));
      // fill select box with # of results
      var querySize = 1000;

      var sizeBox = $('#querySize');
      sizeBox.empty();
      [ 100, 250, 500, 1000, 2500, 5000, 10000, "all" ].forEach(function (value) {
        sizeBox.append('<option value="' + _.escape(value) + '"' +
          (querySize === value ? ' selected' : '') +
          '>' + _.escape(value) + ' results</option>');
      });

      var outputEditor = ace.edit("queryOutput");
      outputEditor.setReadOnly(true);
      outputEditor.setHighlightActiveLine(false);
      outputEditor.getSession().setMode("ace/mode/json");
      outputEditor.setFontSize("13px");
      outputEditor.setValue('');

      var inputEditor = ace.edit("aqlEditor");
      inputEditor.getSession().setMode("ace/mode/aql");
      inputEditor.setFontSize("13px");
      inputEditor.commands.addCommand({
        name: "togglecomment",
        bindKey: {win: "Ctrl-Shift-C", linux: "Ctrl-Shift-C", mac: "Command-Shift-C"},
        exec: function (editor) {
          editor.toggleCommentLines();
        },
        multiSelectAction: "forEach"
      });

      var varsEditor = ace.edit("varsEditor");
      varsEditor.getSession().setMode("ace/mode/aql");
      varsEditor.setFontSize("13px");
      varsEditor.commands.addCommand({
        name: "togglecomment",
        bindKey: {win: "Ctrl-Shift-C", linux: "Ctrl-Shift-C", mac: "Command-Shift-C"},
        exec: function (editor) {
          editor.toggleCommentLines();
        },
        multiSelectAction: "forEach"
      });

      //get cached query if available
      var queryObject = this.getCachedQuery();
      if (queryObject !== null && queryObject !== undefined && queryObject !== "") {
        inputEditor.setValue(queryObject.query);
        if (queryObject.parameter === '' || queryObject === undefined) {
          varsEditor.setValue('{}');
        }
        else {
          varsEditor.setValue(queryObject.parameter);
        }
      }

      var changedFunction = function() {
        var session = inputEditor.getSession(),
        cursor = inputEditor.getCursorPosition(),
        token = session.getTokenAt(cursor.row, cursor.column);
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
        //cache query in localstorage
        var a = inputEditor.getValue(),
        b = varsEditor.getValue();

        if (a.length === 1) {
          a = "";
        }
        if (b.length === 1) {
          b = "";
        }

        self.setCachedQuery(a, b);
      };

      inputEditor.getSession().selection.on('changeCursor', function () {
        changedFunction();
      });

      varsEditor.getSession().selection.on('changeCursor', function () {
        changedFunction();
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

      arangoHelper.fixTooltips(".vars-editor-header i, .queryTooltips, .icon_arangodb", "top");

      $('#aqlEditor .ace_text-input').focus();

      var windowHeight = $(window).height() - 295;
      $('#aqlEditor').height(windowHeight - 100 - 29);
      $('#varsEditor').height(100);
      $('#queryOutput').height(windowHeight);

      inputEditor.resize();
      outputEditor.resize();

      this.initTabArray();
      this.renderSelectboxes();
      this.deselect(varsEditor);
      this.deselect(outputEditor);
      this.deselect(inputEditor);

      // Max: why do we need to tell those elements to show themselves?
      $("#queryDiv").show();
      $("#customsDiv").show();

      this.initQueryImport();

      this.switchTab('query-switch');
      return this;
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
              self.renderSelectboxes();
              self.updateTable();
              self.allowUpload = false;
              $('#customs-switch').click();
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

    downloadQueryResult: function() {
      var inputEditor = ace.edit("aqlEditor");
      var query = inputEditor.getValue();
      if (query !== '' || query !== undefined || query !== null) {
        window.open("query/result/download/" + encodeURIComponent(btoa(JSON.stringify({ query: query }))));
      }
      else {
        arangoHelper.arangoError("Query error", "could not query result.");
      }
    },

    exportCustomQueries: function () {
      var name, toExport = {}, exportArray = [];
      _.each(this.customQueries, function(value) {
        exportArray.push({name: value.name, value: value.value, parameter: value.parameter});
      });
      toExport = {
        "extra": {
          "queries": exportArray
        }
      };

      $.ajax("whoAmI?_=" + Date.now()).success(
        function(data) {
          name = data.user;

          if (name === null || name === false) {
            name = "root";
          }
          window.open("query/download/" + encodeURIComponent(name));
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

      addAQL: function () {
        //update queries first, before showing
        this.refreshAQL(true);

        //render options
        this.createCustomQueryModal();
        $('#new-query-name').val($('#querySelect').val());
        setTimeout(function () {
          $('#new-query-name').focus();
        }, 500);
        this.checkSaveName();
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
      },

      deleteAQL: function (e) {
        var callbackSave = function(error) {
          if (error) {
            arangoHelper.arangoError("Query", "Could not delete query.");
          }
          else {
            this.updateLocalQueries();
            this.renderSelectboxes();
            this.updateTable();
          }
        }.bind(this);

        var deleteName = $(e.target).parent().parent().parent().children().first().text();
        var toDelete = this.collection.findWhere({name: deleteName});

        this.collection.remove(toDelete);
        this.collection.saveCollectionQueries(callbackSave);

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

      saveAQL: function (e) {
        e.stopPropagation();

        //update queries first, before writing
        this.refreshAQL();

        var inputEditor = ace.edit("aqlEditor"),
        varsEditor = ace.edit("varsEditor"),
        saveName = $('#new-query-name').val(),
        bindVars = varsEditor.getValue();

        if ($('#new-query-name').hasClass('invalid-input')) {
          return;
        }

        //Heiko: Form-Validator - illegal query name
        if (saveName.trim() === '') {
          return;
        }

        var content = inputEditor.getValue(),
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

      getCustomQueryValueByName: function (qName) {
        return this.collection.findWhere({name: qName}).get("value");
      },

      getCustomQueryParameterByName: function (qName) {
        return this.collection.findWhere({name: qName}).get("parameter");
      },

      refreshAQL: function(select) {

        var self = this;

        var callback = function(error) {
          if (error) {
            arangoHelper.arangoError('Query', 'Could not reload Queries');
          }
          else {
            self.updateLocalQueries();
            if (select) {
              var previous = $("#querySelect").val();
              self.renderSelectboxes();
              $("#querySelect").val(previous);
            }
          }
        }.bind(self);

        var originCallback = function() {
          self.getSystemQueries(callback);
        }.bind(self);

        this.getAQL(originCallback);
      },

      importSelected: function (e) {
        var inputEditor = ace.edit("aqlEditor"),
        varsEditor = ace.edit("varsEditor");
        _.each(this.queries, function (v) {
          if ($('#' + e.currentTarget.id).val() === v.name) {
            inputEditor.setValue(v.value);

            if (v.hasOwnProperty('parameter')) {
              if (v.parameter === '' || v.parameter === undefined) {
                v.parameter = '{}';
              }
              if (typeof v.parameter === 'object') {
                varsEditor.setValue(JSON.stringify(v.parameter));
              }
              else {
                varsEditor.setValue(v.parameter);
              }
            }
            else {
              varsEditor.setValue('{}');
            }
          }
        });
        _.each(this.customQueries, function (v) {
          if ($('#' + e.currentTarget.id).val() === v.name) {
            inputEditor.setValue(v.value);

            if (v.hasOwnProperty('parameter')) {
              if (v.parameter === '' ||
                  v.parameter === undefined || 
                  JSON.stringify(v.parameter) === '{}') 
              {
                v.parameter = '{}';
              }
              varsEditor.setValue(v.parameter);
            }
            else {
              varsEditor.setValue('{}');
            }
          }
        });
        this.deselect(ace.edit("varsEditor"));
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

      readQueryData: function() {
        var inputEditor = ace.edit("aqlEditor");
        var varsEditor = ace.edit("varsEditor");
        var selectedText = inputEditor.session.getTextRange(inputEditor.getSelectionRange());
        var sizeBox = $('#querySize');
        var data = {
          query: selectedText || inputEditor.getValue(),
          id: "currentFrontendQuery"
        };

        if (sizeBox.val() !== 'all') {
          data.batchSize = parseInt(sizeBox.val(), 10);
        }

        var bindVars = varsEditor.getValue();

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

      heatmapColors: [
        "#313695",
        "#4575b4",
        "#74add1",
        "#abd9e9",
        "#e0f3f8",
        "#ffffbf",
        "#fee090",
        "#fdae61",
        "#f46d43",
        "#d73027",
        "#a50026",
      ],

      heatmap: function(value) {
        return this.heatmapColors[Math.floor(value * 10)];
      },

      followQueryPath: function(root, nodes) {
        var known = {};
        var estCost = 0;
        known[nodes[0].id] = root;
        var i, nodeData, j, dep;
        for (i = 1; i < nodes.length; ++i) {
          nodeData = this.preparePlanNodeEntry(nodes[i], nodes[i-1].estimatedCost);
          known[nodes[i].id] = nodeData;
          dep = nodes[i].dependencies;
          estCost = nodeData.estimatedCost;
          for (j = 0; j < dep.length; ++j) {
            known[dep[j]].children.push(nodeData);
          }
        }
        console.log(estCost);
        return estCost;
      },

      preparePlanNodeEntry: function(node, parentCost) {
        var json = {
          estimatedCost: node.estimatedCost,
          estimatedNrItems: node.estimatedNrItems,
          type: node.type,
          children: []
        };
        switch (node.type) {
          case "SubqueryNode":
            json.relativeCost =  json.estimatedCost - this.followQueryPath(json, node.subquery.nodes);
            break;
          default:
            json.relativeCost = json.estimatedCost - parentCost|| json.estimatedCost;

        }
        return json;
      },
      /*
      drawTree: function() {
        var treeHeight = 0;
        var heatmap = this.heatmap.bind(this);
        if (!this.treeData) {
          return;
        }
        var treeData = this.treeData;
        // outputEditor.setValue(JSON.stringify(treeData, undefined, 2));

        var margin = {top: 20, right: 20, bottom: 20, left: 20},
            width = $("svg#explainOutput").parent().width() - margin.right - margin.left,
            height = 500 - margin.top - margin.bottom;

        var i = 0;
        var maxCost = 0;

        var tree = d3.layout.tree().size([width, height]);

        d3.select("svg#explainOutput g").remove();

        var svg = d3.select("svg#explainOutput")
          .attr("width", width + margin.right + margin.left)
          .attr("height", height + margin.top + margin.bottom)
          .append("g")
          .attr("transform", "translate(" + margin.left + "," + margin.top + ")");

        var root = treeData[0];

        // Compute the new tree layout.
        var nodes = tree.nodes(root).reverse(),
            links = tree.links(nodes);

        var diagonal = d3.svg.diagonal()
          .projection(function(d) { return [d.x, d.y]; });

        // Normalize for fixed-depth.
        nodes.forEach(function(d) { d.y = d.depth * 80 + margin.top; });

        // Declare the nodes¦
        var node = svg.selectAll("g.node")
          .data(nodes, function(d) {
            if (!d.id) {
              d.id = ++i;
            }
            if (treeHeight < d.y) {
              treeHeight = d.y;
            }
            if (maxCost < d.relativeCost) {
              maxCost = d.relativeCost;
            }
            return d.id;
          });

          treeHeight += 60;
          $(".query-output").height(treeHeight);

        // Enter the nodes.
        var nodeEnter = node.enter().append("g")
          .attr("class", "node")
          .attr("transform", function(d) { 
            return "translate(" + d.x + "," + d.y + ")"; });

        nodeEnter.append("circle")
          .attr("r", 10)
          .style("fill", function(d) {return heatmap(d.relativeCost / maxCost);});

        nodeEnter.append("text")
          .attr("dx", "0")
          .attr("dy", "-15")
          .attr("text-anchor", function() {return "middle"; })
          .text(function(d) { return d.type.replace("Node",""); })
          .style("fill-opacity", 1);

        nodeEnter.append("text")
          .attr("dx", "0")
          .attr("dy", "25")
          .attr("text-anchor", function() { return "middle"; })
          .text(function(d) { return "Cost: " + d.relativeCost; })
          .style("fill-opacity", 1);

        // Declare the links¦
        var link = svg.selectAll("path.link")
          .data(links, function(d) { return d.target.id; });

        // Enter the links.
        link.enter().insert("path", "g")
          .attr("class", "link")
          .attr("d", diagonal);

      },
      */

      /*
      showExplainPlan: function(plan) {
        $("svg#explainOutput").html();
        var nodes = plan.nodes;
        if (nodes.length > 0) {
          // Handle root element differently
          var treeData = this.preparePlanNodeEntry(nodes[0]);
          this.followQueryPath(treeData, nodes);
          this.treeData = [treeData];
          this.drawTree();
        }
      },
      */

     /*
      showExplainWarnings: function(warnings) {
        $(".explain-warnings").html(this.warningTemplate.render({warnings: warnings}));
      },
      */

      fillExplain: function(callback) {

        var self = this;
        var outputEditor = ace.edit("queryOutput");
        var queryData = this.readQueryData();
        $('.queryExecutionTime').text('');
        this.execPending = false;

        if (queryData) {
          window.progressView.show(
            "Explain is operating..."
          );

          $.ajax({
            type: "POST",
            url: arangoHelper.databaseUrl("/_admin/aardvark/query/explain/"),
            data: queryData,
            contentType: "application/json",
            processData: false,
            success: function (data) {
              outputEditor.setValue(data.msg);
              self.switchTab("result-switch");
              window.progressView.hide();
              self.deselect(outputEditor);
              $('#downloadQueryResult').hide();
              if (typeof callback === "function") {
                callback();
              }
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
              if (typeof callback === "function") {
                callback();
              }
            }
          });
        }
      /*
        var self = this;
        $("svg#explainOutput").html();
        $.ajax({
          type: "POST",
          url: frontendConfig.basePath + "/_api/explain",
          data: this.readQueryData(),
          contentType: "application/json",
          processData: false,
          success: function (data) {
            if (typeof callback === "function") {
              callback();
            }
            self.showExplainWarnings(data.warnings);
            self.showExplainPlan(data.plan);
          },
          error: function (errObj) {
            var res = errObj.responseJSON;
            // Display ErrorMessage
          }
        });
      */
      },

      resize: function() {
        // this.drawTree();
      },

      checkQueryTimer: undefined,

      queryCallbackFunction: function(queryID, callback) {

        var self = this;
        var outputEditor = ace.edit("queryOutput");

        var cancelRunningQuery = function() {

          $.ajax({
            url: arangoHelper.databaseUrl('/_api/job/'+ encodeURIComponent(queryID) + "/cancel"),
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

        $('.queryExecutionTime').text('');
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
          self.switchTab("result-switch");
          window.progressView.hide();
          
          var time = "-";
          if (data && data.extra && data.extra.stats) {
            time = data.extra.stats.executionTime.toFixed(3) + " s";
          }

          $('.queryExecutionTime').text("Execution time: " + time);

          self.deselect(outputEditor);
          $('#downloadQueryResult').show();

          if (typeof callback === "function") {
            callback();
          }
        };

        //check if async query is finished
        var checkQueryStatus = function() {
          $.ajax({
            type: "PUT",
            url: arangoHelper.databaseUrl("/_api/job/" + encodeURIComponent(queryID)),
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

      fillResult: function(callback) {
        var self = this;
        var outputEditor = ace.edit("queryOutput");
        // clear result
        outputEditor.setValue('');

        var queryData = this.readQueryData();
        if (queryData) {
          $.ajax({
            type: "POST",
            url: arangoHelper.databaseUrl("/_api/cursor"),
            headers: {
              'x-arango-async': 'store' 
            },
            data: queryData,
            contentType: "application/json",
            processData: false,
            success: function (data, textStatus, xhr) {
              if (xhr.getResponseHeader('x-arango-async-id')) {
                self.queryCallbackFunction(xhr.getResponseHeader('x-arango-async-id'), callback);
              }
              $.noty.clearQueue();
              $.noty.closeAll();
            },
            error: function (data) {
              self.switchTab("result-switch");
              $('#downloadQueryResult').hide();
              try {
                var temp = JSON.parse(data.responseText);
                outputEditor.setValue('[' + temp.errorNum + '] ' + temp.errorMessage);
                //arangoHelper.arangoError("Query error", temp.errorMessage);
              }
              catch (e) {
                outputEditor.setValue('ERROR');
                arangoHelper.arangoError("Query error", "ERROR");
              }
              window.progressView.hide();
              if (typeof callback === "function") {
                callback();
              }
            }
          });
        }
      },

      submitQuery: function () {
        var outputEditor = ace.edit("queryOutput");
        this.fillResult(this.switchTab.bind(this, "result-switch"));
        outputEditor.resize();
        var inputEditor = ace.edit("aqlEditor");
        this.deselect(inputEditor);
        $('#downloadQueryResult').show();
      },

      explainQuery: function() {
        this.fillExplain();
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
        var switchId;
        if (typeof e === 'string') {
          switchId = e;
        } else {
          switchId = e.target.id;
        }
        var self = this;
        var changeTab = function (element){
          var divId = "#" + element.replace("-switch", "");
          var contentDivId = "#tabContent" + divId.charAt(1).toUpperCase() + divId.substr(2);
          if (element === switchId) {
            $("#" + element).parent().addClass("active");
            $(divId).addClass("active");
            $(contentDivId).show();
            if (switchId === 'query-switch') {
              // issue #1000: set focus to query input
              $('#aqlEditor .ace_text-input').focus();
            } else if (switchId === 'result-switch' && self.execPending) {
              self.fillResult();
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
