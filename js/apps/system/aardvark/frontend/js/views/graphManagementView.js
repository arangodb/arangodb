/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, $, _, window, templateEngine, arangoHelper*/

(function() {
  "use strict";

  window.GraphManagementView = Backbone.View.extend({
    el: '#content',
    template: templateEngine.createTemplate("graphManagementView.ejs"),

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
      return this;
    },

    editGraph : function(e) {
      this.collection.fetch();
      this.graphToEdit = this.evaluateGraphName($(e.currentTarget).attr("id"), '_settings');
      var graph = this.collection.findWhere({_key: this.graphToEdit});
      this.createEditGraphModal2(this.graphToEdit, graph.get("vertices"), graph.get("edges"));
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
      var _key = $("#createNewGraphName").val(),
        vertexCollections = [],
        edgeDefinitions = [],
        self = this;
      if (!_key) {
        arangoHelper.arangoNotification(
          "A name for the graph has to be provided."
        );
        return;
      }
      this.collection.create({
        _key: _key,
        edgeDefinitions: edgeDefinitions,
        vertexCollections: vertexCollections
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
      var id = $(e.currentTarget).attr("id");
      if (id == "row_newEdgeDefinitions1") {
        $('#row_fromCollections1').toggle();
        $('#row_toCollections1').toggle();
      }
    },


    createNewGraphModal2: function() {
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
          "newEdgeDefinitions1",
          "Edge definitions",
          "",
          "Some info for edge definitions",
          "Edge definitions",
          true
        )
      );

      tableContent.push(
        window.modalView.createSelect2Entry(
          "fromCollections1",
          "fromCollections",
          "",
          "The collection that contain the start vertices of the relation.",
          "",
          true
        )
      );
      tableContent.push(
        window.modalView.createSelect2Entry(
          "toCollections1",
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
        window.modalView.createSuccessButton("Create", this.createNewGraph.bind(this))
      );


      window.modalView.show("modalTable.ejs", "Add new Graph", buttons, tableContent, null, this.events);
      $('#row_fromCollections1').hide();
      $('#row_toCollections1').hide();
    },

    createEditGraphModal2: function(name, vertices, edges) {
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

    }

  });
}());
