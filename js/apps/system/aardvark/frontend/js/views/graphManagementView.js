/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, $, _, window, templateEngine, arangoHelper, GraphViewerUI, require */

(function() {
  "use strict";

  window.GraphManagementView = Backbone.View.extend({
    el: '#content',
    template: templateEngine.createTemplate("graphManagementView.ejs"),
    edgeDefintionTemplate: templateEngine.createTemplate("edgeDefinitionTable.ejs"),

    events: {
      "click #deleteGraph"                  : "deleteGraph",
      "click .icon_arangodb_settings2"      : "editGraph",
      "click .icon_arangodb_info"           : "info",
      "click #createGraph"                  : "addNewGraph",
      "keyup #graphManagementSearchInput"   : "search",
      "click #graphManagementSearchSubmit"  : "search",
      "click .tile-graph"                   : "loadGraphViewer"
    },

    loadGraphViewer: function(e) {
      var name = $(e.currentTarget).attr("id");
      name = name.substr(0, name.length - 5);
      var adapterConfig = {
        type: "gharial",
        graphName: name,
        baseUrl: require("internal").arango.databasePrefix("/")
      };
      var width = $("#content").width() - 75;
      $("#content").html("");
      this.ui = new GraphViewerUI($("#content")[0], adapterConfig, width, 680, {
        nodeShaper: {
          label: "_key",
          color: {
            type: "attribute",
            key: "_key"
          }
        }

      }, true);
    },

    addNewGraph: function(e) {
      e.preventDefault();
      this.createNewGraphModal();
    },

    deleteGraph: function(e) {
      var self = this;
      var name = $('#editGraphName').html();
      this.collection.get(name).destroy({
        success: function() {
          self.updateGraphManagementView();
          window.modalView.hide();
        },
        error: function(xhr, err) {
          var response = JSON.parse(err.responseText),
            msg = response.errorMessage;
          arangoHelper.arangoError(msg);
          window.modalView.hide();
        }
      });
    },

    render: function() {
      this.collection.fetch({
        async: false
      });

      $(this.el).html(this.template.render({
        graphs: this.collection,
        searchString : ''
      }));
      this.events["click .tableRow"] = this.showHideDefinition.bind(this);
      this.events['change [id*="newEdgeDefinitions"]'] = this.setFromAndTo.bind(this);
      this.events["click .graphViewer-icon-button"] = this.addRemoveDefinition.bind(this);

      return this;
    },

    setFromAndTo : function (e) {
      var map = this.calculateEdgeDefinitionMap(), id;
      if (map[e.val]) {
        id = e.currentTarget.id.split("row_newEdgeDefinitions")[1];
        $('#s2id_fromCollections'+id).select2("val", map[e.val].from);
        $('#fromCollections'+id).attr('disabled', true);
        $('#s2id_toCollections'+id).select2("val", map[e.val].to);
        $('#toCollections'+id).attr('disabled', true);
      } else {
        id = e.currentTarget.id.split("row_newEdgeDefinitions")[1];
        $('#s2id_fromCollections'+id).select2("val", null);
        $('#fromCollections'+id).attr('disabled', false);
        $('#s2id_toCollections'+id).select2("val", null);
        $('#toCollections'+id).attr('disabled', false);
      }
    },

    editGraph : function(e) {
      e.stopPropagation();
      this.collection.fetch();
      this.graphToEdit = this.evaluateGraphName($(e.currentTarget).attr("id"), '_settings');
      var graph = this.collection.findWhere({_key: this.graphToEdit});
      this.createEditGraphModal(
        this.graphToEdit, graph.get("edgeDefinitions"), graph.get("orphanCollections")
      );
    },

    info : function(e) {
      e.stopPropagation();
      this.collection.fetch();
      var graph = this.collection.findWhere(
        {_key: this.evaluateGraphName($(e.currentTarget).attr("id"), '_info')}
      );
      this.createInfoGraphModal(
        graph.get('_key'),
        graph.get('vertices'),
        graph.get('edges')
      );
    },

    saveEditedGraph: function() {
      var name = $("#editGraphName").html(),
        vertexCollections = _.pluck($('#newVertexCollections').select2("data"), "text"),
        edgeDefinitions = [],
        newEdgeDefinitions = {},
        self = this,
        collection,
        from,
        to,
        index,
        edgeDefinitionElements;

      edgeDefinitionElements = $('[id^=s2id_newEdgeDefinitions]').toArray();
      edgeDefinitionElements.forEach(
        function(eDElement) {
          index = $(eDElement).attr("id");
          index = index.replace("s2id_newEdgeDefinitions", "");
          collection = _.pluck($('#s2id_newEdgeDefinitions' + index).select2("data"), "text")[0];
          if (collection && collection !== "") {
            from = _.pluck($('#s2id_newFromCollections' + index).select2("data"), "text");
            to = _.pluck($('#s2id_newToCollections' + index).select2("data"), "text");
            if (from !== 1 && to !== 1) {
              var edgeDefinition = {
                collection: collection,
                from: from,
                to: to
              };
              edgeDefinitions.push(edgeDefinition);
              newEdgeDefinitions[collection] = edgeDefinition;
            }
          }
        }
      );

      //get current edgeDefs/orphanage
      var graph = this.collection.findWhere({_key: name});
      var currentEdgeDefinitions = graph.get("edgeDefinitions");
      var currentOrphanage = graph.get("orphanCollections");
      var currentCollections = [];


      //evaluate all new, edited and deleted edge definitions
      var newEDs = [];
      var editedEDs = [];
      var deletedEDs = [];

      currentEdgeDefinitions.forEach(
        function(eD) {
          var collection = eD.collection;
          currentCollections.push(collection);
          var newED = newEdgeDefinitions[collection];
          if (newED === undefined) {
            deletedEDs.push(collection);
          } else if (JSON.stringify(newED) !== JSON.stringify(eD)) {
            editedEDs.push(collection);
          }
        }
      );
      edgeDefinitions.forEach(
        function(eD) {
          var collection = eD.collection;
          if (currentCollections.indexOf(collection) === -1) {
            newEDs.push(collection);
          }
        }
      );

      newEDs.forEach(
        function(eD) {
          graph.addEdgeDefinition(newEdgeDefinitions[eD]);
        }
      );

      editedEDs.forEach(
        function(eD) {
          graph.modifyEdgeDefinition(newEdgeDefinitions[eD]);
        }
      );

      deletedEDs.forEach(
        function(eD) {
          graph.deleteEdgeDefinition(eD);
        }
      );

      this.updateGraphManagementView();
    },

    evaluateGraphName : function(str, substr) {
      var index = str.lastIndexOf(substr);
      return str.substring(0, index);
    },

    search: function() {
      var searchInput,
        searchString,
        strLength,
        reducedCollection;

      searchInput = $('#graphManagementSearchInput');
      searchString = $("#graphManagementSearchInput").val();
      reducedCollection = this.collection.filter(
        function(u) {
          return u.get("_key").indexOf(searchString) !== -1;
        }
      );
      $(this.el).html(this.template.render({
        graphs        : reducedCollection,
        searchString  : searchString
      }));

      //after rendering, get the "new" element
      searchInput = $('#graphManagementSearchInput');
      //set focus on end of text in input field
      strLength= searchInput.val().length;
      searchInput.focus();
      searchInput[0].setSelectionRange(strLength, strLength);
    },

    updateGraphManagementView: function() {
      var self = this;
      this.collection.fetch({
        success: function() {
          self.render();
        }
      });
    },

    createNewGraph: function() {
      var name = $("#createNewGraphName").val(),
        vertexCollections = _.pluck($('#newVertexCollections').select2("data"), "text"),
        edgeDefinitions = [],
        self = this,
        collection,
        from,
        to,
        index,
        edgeDefinitionElements;


      edgeDefinitionElements = $('[id^=s2id_newEdgeDefinitions]').toArray();
      edgeDefinitionElements.forEach(
        function(eDElement) {
          index = $(eDElement).attr("id");
          index = index.replace("s2id_newEdgeDefinitions", "");
          collection = _.pluck($('#s2id_newEdgeDefinitions' + index).select2("data"), "text")[0];
          if (collection && collection !== "") {
            from = _.pluck($('#s2id_fromCollections' + index).select2("data"), "text");
            to = _.pluck($('#s2id_toCollections' + index).select2("data"), "text");
            if (from !== 1 && to !== 1) {
              edgeDefinitions.push(
                {
                  collection: collection,
                  from: from,
                  to: to
                }
              );
            }
          }
        }
      );

      if (!name) {
        arangoHelper.arangoNotification(
          "A name for the graph has to be provided."
        );
        return;
      }
      this.collection.create({
        name: name,
        edgeDefinitions: edgeDefinitions,
        orphanCollections: vertexCollections
      }, {
        success: function() {
          self.updateGraphManagementView();
          window.modalView.hide();
        },
        error: function(obj, err) {
          var response = JSON.parse(err.responseText),
            msg = response.errorMessage;
          // Gritter does not display <>
          msg = msg.replace("<", "");
          msg = msg.replace(">", "");
          arangoHelper.arangoError(msg);
        }
      });
    },

    createEditGraphModal: function(name, edgeDefinitions, orphanCollections) {
      var buttons = [],
        tableContent = [],
        maxIndex;
      this.counter = 0;
      window.modalView.disableSubmitOnEnter = true;

      tableContent.push(
        window.modalView.createReadOnlyEntry(
          "editGraphName",
          "Name",
          name,
          false
        )
      );

      edgeDefinitions.forEach(
        function(edgeDefinition, index) {
          maxIndex = index;
          if (index === 0) {
            tableContent.push(
              window.modalView.createSelect2Entry(
                "newEdgeDefinitions" + index,
                "Edge definitions",
                edgeDefinition.collection,
                "Some info for edge definitions",
                "Edge definitions",
                true,
                true,
                true,
                1
              )
            );
          } else {
            tableContent.push(
              window.modalView.createSelect2Entry(
                "newEdgeDefinitions" + index,
                "Edge definitions",
                edgeDefinition.collection,
                "Some info for edge definitions",
                "Edge definitions",
                true,
                true,
                false,
                1
              )
            );
          }
          tableContent.push(
            window.modalView.createSelect2Entry(
              "fromCollections" + index,
              "fromCollections",
              edgeDefinition.from,
              "The collection that contain the start vertices of the relation.",
              "fromCollections",
              true,
              false,
              false,
              10
            )
          );
          tableContent.push(
            window.modalView.createSelect2Entry(
              "toCollections" + index,
              "toCollections",
              edgeDefinition.to,
              "The collection that contain the end vertices of the relation.",
              "toCollections",
              true,
              false,
              false,
              10
            )
          );
        }
      );

      tableContent.push(
        window.modalView.createSelect2Entry(
          "newVertexCollections",
          "Vertex collections",
          orphanCollections,
          "Some info for vertex collections",
          "Vertex Collections",
          false
        )
      );
      buttons.push(
        window.modalView.createDeleteButton("Delete", this.deleteGraph.bind(this))
      );
      buttons.push(
        window.modalView.createSuccessButton("Save", this.saveEditedGraph.bind(this))
      );


      window.modalView.show(
        "modalGraphTable.ejs", "Add new Graph", buttons, tableContent, null, this.events
      );

      var i;
      for (i = 0; i <= maxIndex; i++) {
        $('#row_fromCollections' + i).hide();
        $('#row_toCollections' + i).hide();
      }

    },

    createInfoGraphModal: function(name, vertices, edges) {
      var buttons = [],
        tableContent = [];

      tableContent.push(
        window.modalView.createReadOnlyEntry(
          "infoGraphName",
          "Name",
          name,
          false
        )
      );
      tableContent.push(
        window.modalView.createReadOnlyEntry(
          "infoVertices",
          "Vertices",
          vertices,
          false
        )
      );
      tableContent.push(
        window.modalView.createReadOnlyEntry(
          "infoEdges",
          "Edges",
          edges,
          false
        )
      );

      window.modalView.show("modalTable.ejs", "Graph Properties", buttons, tableContent);

    },

    showHideDefinition : function(e) {
      e.stopPropagation();
      var id = $(e.currentTarget).attr("id"), number;
      if (id.indexOf("row_newEdgeDefinitions") !== -1 ) {
        number = id.split("row_newEdgeDefinitions")[1];
        $('#row_fromCollections' + number).toggle();
        $('#row_toCollections' + number).toggle();
      }
    },

    addRemoveDefinition : function(e) {

      var collList = [], eCollList = [],
        collections = this.options.collectionCollection.models;

      collections.forEach(function (c) {
        if (c.get("isSystem")) {
          return;
        }
        collList.push(c.id);
        if (c.get('type') === "edge") {
          eCollList.push(c.id);
        }
      });
      e.stopPropagation();
      var id = $(e.currentTarget).attr("id"), number;
      if (id.indexOf("addAfter_newEdgeDefinitions") !== -1 ) {
        this.counter++;
        $('#row_newVertexCollections').before(
          this.edgeDefintionTemplate.render({
            number: this.counter
          })
        );
        $('#newEdgeDefinitions'+this.counter).select2({
          tags: eCollList,
          showSearchBox: false,
          minimumResultsForSearch: -1,
          width: "336px",
          maximumSelectionSize: 1
        });
        $('#fromCollections'+this.counter).select2({
          tags: collList,
          showSearchBox: false,
          minimumResultsForSearch: -1,
          width: "336px",
          maximumSelectionSize: 10
        });
        $('#toCollections'+this.counter).select2({
          tags: collList,
          showSearchBox: false,
          minimumResultsForSearch: -1,
          width: "336px",
          maximumSelectionSize: 10
        });
        window.modalView.undelegateEvents();
        window.modalView.delegateEvents(this.events);
        return;
      }
      if (id.indexOf("remove_newEdgeDefinitions") !== -1 ) {
        number = id.split("remove_newEdgeDefinitions")[1];
        $('#row_newEdgeDefinitions' + number).remove();
        $('#row_fromCollections' + number).remove();
        $('#row_toCollections' + number).remove();
      }
    },

    calculateEdgeDefinitionMap : function () {
      var edgeDefinitionMap = {};
      this.collection.models.forEach(function(m) {
        m.get("edgeDefinitions").forEach(function (ed) {
          edgeDefinitionMap[ed.collection] = {
            from : ed.from,
            to : ed.to
          };
        });
      });
      return edgeDefinitionMap;
    },

    createNewGraphModal: function() {
      var buttons = [], collList = [], eCollList = [],
        tableContent = [], collections = this.options.collectionCollection.models;

      collections.forEach(function (c) {
        if (c.get("isSystem")) {
          return;
        }
        collList.push(c.id);
        if (c.get('type') === "edge") {

          eCollList.push(c.id);
        }
      });
      this.counter = 0;
      window.modalView.enableHotKeys = false;

      tableContent.push(
        window.modalView.createTextEntry(
          "createNewGraphName",
          "Name",
          "",
          "The name to identify the graph. Has to be unique.",
          "graphName",
          true
        )
      );

      tableContent.push(
        window.modalView.createSelect2Entry(
          "newEdgeDefinitions0",
          "Edge definitions",
          "",
          "Some info for edge definitions",
          "Edge definitions",
          true,
          false,
          true,
          1,
          eCollList
        )
      );
      tableContent.push(
        window.modalView.createSelect2Entry(
          "fromCollections0",
          "fromCollections",
          "",
          "The collection that contain the start vertices of the relation.",
          "fromCollections",
          true,
          false,
          false,
          10,
          collList
        )
      );
      tableContent.push(
        window.modalView.createSelect2Entry(
          "toCollections0",
          "toCollections",
          "",
          "The collection that contain the end vertices of the relation.",
          "toCollections",
          true,
          false,
          false,
          10,
          collList
        )
      );


      tableContent.push(
        window.modalView.createSelect2Entry(
          "newVertexCollections",
          "Vertex collections",
          "",
          "Some info for vertex collections",
          "Vertex Collections",
          false,
          false,
          false,
          10,
          collList
        )
      );
      buttons.push(
        window.modalView.createSuccessButton("Create", this.createNewGraph.bind(this))
      );


      window.modalView.show(
        "modalGraphTable.ejs", "Add new Graph", buttons, tableContent, null, this.events
      );
      $('#row_fromCollections1').hide();
      $('#row_toCollections1').hide();
    },

    createEditGraphModal2: function(name, vertices, edgeDefinitions) {
      var buttons = [],
        tableContent = [];

      this.counter = 0;

      tableContent.push(
        window.modalView.createReadOnlyEntry(
          "graphName",
          "Name",
          name,
          "The name to identify the graph. Has to be unique."
        )
      );
      edgeDefinitions.forEach(function(ed) {
        tableContent.push(
          window.modalView.createReadOnlyEntry(
            "edgeDefinitions" + this.counter,
            "Edge definitions",
            ed.collection,
            "Some info for edge definitions"
          )
        );

        tableContent.push(
          window.modalView.createSelect2Entry(
            "fromCollections" + this.counter,
            "fromCollections",
            ed.from,
            "The collection that contain the start vertices of the relation.",
            "",
            true
          )
        );
        tableContent.push(
          window.modalView.createSelect2Entry(
            "toCollections" + this.counter,
            "toCollections",
            ed.to,
            "The collection that contain the end vertices of the relation.",
            "",
            true
          )
        );
        this.counter++;
      });

      tableContent.push(
        window.modalView.createSelect2Entry(
          "vertexCollections",
          "Vertex collections",
          vertices,
          "Some info for vertex collections",
          "Vertex Collections",
          false
        )
      );

      buttons.push(
        window.modalView.createDeleteButton("Delete", this.deleteGraph.bind(this))
      );
      buttons.push(
        window.modalView.createNeutralButton("Save", this.deleteGraph.bind(this))
      );

      window.modalView.show("modalGraphTable.ejs", "Edit Graph", buttons, tableContent);

    }

  });
}());
