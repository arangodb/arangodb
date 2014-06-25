/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, $, _, window, templateEngine, arangoHelper*/

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
      "click #graphManagementSearchSubmit"  : "search"
    },

    addNewGraph: function(e) {
      e.preventDefault();
      this.createNewGraphModal2();
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
      this.events["click .graphViewer-icon-button"] = this.addRemoveDefinition.bind(this);
      return this;
    },

    editGraph : function(e) {
      this.collection.fetch();
      this.graphToEdit = this.evaluateGraphName($(e.currentTarget).attr("id"), '_settings');
      var graph = this.collection.findWhere({_key: this.graphToEdit});
      this.createEditGraphModal(this.graphToEdit, graph.get("vertices"), graph.get("edges"));
    },

    info : function(e) {
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
      var _key = $("#createNewGraphName").val(),
        vertices = $("#newGraphVertices").val(),
        edges = $("#newGraphEdges").val(),
        self = this;
      if (!_key) {
        arangoHelper.arangoNotification(
          "A name for the graph has to be provided."
        );
        return;
      }
      if (!vertices) {
        arangoHelper.arangoNotification(
          "A vertex collection has to be provided."
        );
        return;
      }
      if (!edges) {
        arangoHelper.arangoNotification(
          "An edge collection has to be provided."
        );
        return;
      }
      this.collection.create({
        _key: _key,
        vertices: vertices,
        edges: edges
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

    createNewGraph2: function() {
      var name = $("#createNewGraphName").val(),
        vertexCollections = _.pluck($('#newVertexCollections').select2("data"), "text"),
        edgeDefinitions = [],
        self = this,
        hasNext = true,
        collection,
        from,
        to;

      collection = $('#newEdgeDefinitions1').val();
      //collection = $("tr[id*='newEdgeDefinitions'").val();
      if (collection !== "") {
        from = _.pluck($('#s2id_fromCollections1').select2("data"), "text");
        to = _.pluck($('#s2id_toCollections1').select2("data"), "text");
        edgeDefinitions.push(
          {
            collection: collection,
            from: from,
            to: to
          }
        );
      }

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

    createNewGraphModal: function() {
      var buttons = [],
        tableContent = [];

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
        window.modalView.createTextEntry(
          "newGraphVertices",
          "Vertices",
          "",
          "The path name of the document collection that should be used as vertices. "
            + "If this does not exist it will be created.",
          "Vertex Collection",
          true
        )
      );
      tableContent.push(
        window.modalView.createTextEntry(
          "newGraphEdges",
          "Edges",
          "",
          "The path name of the edge collection that should be used as edges. "
            + "If this does not exist it will be created.",
          "Edge Collection",
          true
        )
      );
      buttons.push(
        window.modalView.createSuccessButton("Create", this.createNewGraph.bind(this))
      );

      window.modalView.show("modalTable.ejs", "Add new Graph", buttons, tableContent);
    },

    createEditGraphModal: function(name, vertices, edges) {
      var buttons = [],
        tableContent = [];

      tableContent.push(
        window.modalView.createReadOnlyEntry(
          "editGraphName",
          "Name",
          name,
          false
        )
      );
      tableContent.push(
        window.modalView.createReadOnlyEntry(
          "editVertices",
          "Vertices",
          vertices,
          false
        )
      );
      tableContent.push(
        window.modalView.createReadOnlyEntry(
          "editEdges",
          "Edges",
          edges,
          false
        )
      );

      buttons.push(
        window.modalView.createDeleteButton("Delete", this.deleteGraph.bind(this))
      );

      window.modalView.show("modalTable.ejs", "Edit Graph", buttons, tableContent);

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
      e.stopPropagation();
      var id = $(e.currentTarget).attr("id"), number;
      if (id.indexOf("addAfter_newEdgeDefinitions") !== -1 ) {
        this.counter++;
        $('#row_newVertexCollections').before(
          this.edgeDefintionTemplate.render({
            number: this.counter
          })
        );
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

    createNewGraphModal2: function() {
      var buttons = [],
        tableContent = [];

      this.counter = 0;

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
          true
        )
      );

      tableContent.push(
        window.modalView.createSelect2Entry(
          "fromCollections0",
          "fromCollections",
          "",
          "The collection that contain the start vertices of the relation.",
          "",
          true
        )
      );
      tableContent.push(
        window.modalView.createSelect2Entry(
          "toCollections0",
          "toCollections",
          "",
          "The collection that contain the end vertices of the relation.",
          "",
          true
        )
      );


      tableContent.push(
        window.modalView.createSelect2Entry(
          "newVertexCollections",
          "Vertex collections",
          "",
          "Some info for vertex collections",
          "Vertex Collections",
          false
        )
      );
      buttons.push(
        window.modalView.createSuccessButton("Create", this.createNewGraph2.bind(this))
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
