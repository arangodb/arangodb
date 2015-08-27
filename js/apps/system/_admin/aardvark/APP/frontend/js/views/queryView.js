/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, EJS, $, setTimeout, localStorage, ace, Storage, window, _, console */
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
//      'click #explain-switch': "switchTab",
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
      var inputEditor = ace.edit("aqlEditor");
      this.setCachedQuery(inputEditor.getValue());
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
      var self = this;
      this.$el.html(this.template.render({}));
      this.$(this.id).html(this.table.render({content: this.tableDescription}));
      // fill select box with # of results
      var querySize = 1000;

      var sizeBox = $('#querySize');
      sizeBox.empty();
      [ 100, 250, 500, 1000, 2500, 5000, 10000 ].forEach(function (value) {
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

      //get cached query if available
      var query = this.getCachedQuery();
      if (query !== null && query !== undefined && query !== "") {
        inputEditor.setValue(query);
      }

      inputEditor.getSession().selection.on('changeCursor', function () {
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
        //cache query in localstorage
        self.setCachedQuery(inputEditor.getValue());
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

    getCachedQuery: function() {
      if(typeof(Storage) !== "undefined") {
        var cache = localStorage.getItem("cachedQuery");
        if (cache !== undefined) {
          var query = JSON.parse(cache);
          return query;
        }
      }
    },

    setCachedQuery: function(query) {
      if(typeof(Storage) !== "undefined") {
        localStorage.setItem("cachedQuery", JSON.stringify(query));
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
          this.collection.fetch({async: false});
          this.updateLocalQueries();
          this.renderSelectboxes();
          this.updateTable();
          self.allowUpload = false;
          $('#customs-switch').click();
        };

        self.collection.saveImportQueries(self.file, callback.bind(this));
        $('#confirmQueryImport').addClass('disabled');
        $('#queryImportDialog').modal('hide'); 
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
      _.each(this.customQueries, function(value, key) {
        exportArray.push({name: value.name, value: value.value});
      });
      toExport = {
        "extra": {
          "queries": exportArray
        }
      };

      $.ajax("whoAmI?_=" + Date.now(), {async:false}).done(
        function(data) {
          name = data.user;

          if (name === null || name === false) {
            name = "root";
          }

        });

        window.open("query/download/" + encodeURIComponent(name));
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

      getAQL: function () {
        var self = this, result;

        this.collection.fetch({
          async: false
        });

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

        //update queries first, before writing
        this.refreshAQL();

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

      refreshAQL: function(select) {
        this.getAQL();
        this.getSystemQueries();
        this.updateLocalQueries();

        if (select) {
          var previous = $("#querySelect" ).val();
          this.renderSelectboxes();
          $("#querySelect" ).val(previous);
        }
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

      readQueryData: function() {
        var inputEditor = ace.edit("aqlEditor");
        var selectedText = inputEditor.session.getTextRange(inputEditor.getSelectionRange());
        var sizeBox = $('#querySize');
        var data = {
          query: selectedText || inputEditor.getValue(),
          batchSize: parseInt(sizeBox.val(), 10),
          id: "currentFrontendQuery"
        };
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

      resize: function() {
        // this.drawTree();
      },

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
        $('.queryExecutionTime').text('');
        window.progressView.show(
          "Explain is operating..."
        );
        this.execPending = false;
        $.ajax({
          type: "POST",
          url: "/_admin/aardvark/query/explain/",
          data: this.readQueryData(),
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
          },
          error: function (data) {
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

      /*
        var self = this;
        $("svg#explainOutput").html();
        $.ajax({
          type: "POST",
          url: "/_api/explain",
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
            console.log("Error:", res.errorMessage);
          }
        });
      */
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

      fillResult: function(callback) {
        var self = this;
        var outputEditor = ace.edit("queryOutput");
        // clear result
        outputEditor.setValue('');

        window.progressView.show(
          "Query is operating..."
        );

        $('.queryExecutionTime').text('');
        self.timer.start();

        this.execPending = false;
        $.ajax({
          type: "POST",
          url: "/_api/cursor",
          data: this.readQueryData(),
          contentType: "application/json",
          processData: false,
          success: function (data) {

            var time = "Execution time: " + self.timer.getTimeAndReset()/1000 + " s";
            $('.queryExecutionTime').text(time);

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
            self.switchTab("result-switch");
            window.progressView.hide();
            self.deselect(outputEditor);
            $('#downloadQueryResult').show();
            if (typeof callback === "function") {
              callback();
            }
          },
          error: function (data) {
            self.switchTab("result-switch");
            $('#downloadQueryResult').hide();
            try {
              var temp = JSON.parse(data.responseText);
              outputEditor.setValue('[' + temp.errorNum + '] ' + temp.errorMessage);
              arangoHelper.arangoError("Query error", temp.errorNum, temp.errorMessage);
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
      },

      submitQuery: function () {
        var outputEditor = ace.edit("queryOutput");
        // this.fillExplain();
        this.fillResult(this.switchTab.bind(this, "result-switch"));
        outputEditor.resize();
        var inputEditor = ace.edit("aqlEditor");
        this.deselect(inputEditor);
        $('#downloadQueryResult').show();
      },

      explainQuery: function() {

        this.fillExplain();
      /*
        this.fillExplain(this.switchTab.bind(this, "explain-switch"));
        this.execPending = true;
        var inputEditor = ace.edit("aqlEditor");
        this.deselect(inputEditor);
      */
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
