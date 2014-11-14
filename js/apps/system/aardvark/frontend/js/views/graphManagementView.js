/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, $, _, window, templateEngine, arangoHelper, GraphViewerUI, require */

(function() {
  "use strict";

  window.GraphManagementView = Backbone.View.extend({
    el: '#content',
    template: templateEngine.createTemplate("graphManagementView.ejs"),
    edgeDefintionTemplate: templateEngine.createTemplate("edgeDefinitionTable.ejs"),
    eCollList : [],
    removedECollList : [],

    dropdownVisible: false,

    events: {
      "click #deleteGraph"                        : "deleteGraph",
      "click .icon_arangodb_settings2.editGraph"  : "editGraph",
      "click #createGraph"                        : "addNewGraph",
      "keyup #graphManagementSearchInput"         : "search",
      "click #graphManagementSearchSubmit"        : "search",
      "click .tile-graph"                         : "loadGraphViewer",
      "click #graphManagementToggle"              : "toggleGraphDropdown",
      "click .css-label"                          : "checkBoxes",
      "change #graphSortDesc"                     : "sorting"
    },

    loadGraphViewer: function(e) {
      var name = $(e.currentTarget).attr("id");
      name = name.substr(0, name.length - 5);
      var edgeDefs = this.collection.get(name).get("edgeDefinitions");
      if (!edgeDefs || edgeDefs.length === 0) {
        // User Info
        return;
      }
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

      //decorate hash. is needed for router url management
      window.location.hash = "#graphdetail";
    },

    handleResize: function(w) {
      if (!this.width || this.width !== w) {
        this.width = w;
        if (this.ui) {
          this.ui.changeWidth(w);
        }
      }
    },

    addNewGraph: function(e) {
      e.preventDefault();
      this.createEditGraphModal();
    },

    deleteGraph: function() {
      var self = this;
      var name = $("#editGraphName")[0].value;
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

    checkBoxes: function (e) {
      //chrome bugfix
      var clicked = e.currentTarget.id;
      $('#'+clicked).click();
    },

    toggleGraphDropdown: function() {
      //apply sorting to checkboxes
      $('#graphSortDesc').attr('checked', this.collection.sortOptions.desc);

      $('#graphManagementToggle').toggleClass('activated');
      $('#graphManagementDropdown2').slideToggle(200);
    },

    sorting: function() {
      if ($('#graphSortDesc').is(":checked")) {
        this.collection.setSortingDesc(true);
      }
      else {
        this.collection.setSortingDesc(false);
      }

      if ($('#graphManagementDropdown').is(":visible")) {
        this.dropdownVisible = true;
      } else {
        this.dropdownVisible = false;
      }

      this.render();
    },

    render: function() {
      this.collection.fetch({
        async: false
      });

      this.collection.sort();

      $(this.el).html(this.template.render({
        graphs: this.collection,
        searchString : ''
      }));

      if (this.dropdownVisible === true) {
        $('#graphManagementDropdown2').show();
        $('#graphSortDesc').attr('checked', this.collection.sortOptions.desc);
        $('#graphManagementToggle').toggleClass('activated');
        $('#graphManagementDropdown').show();
      }

      this.events["click .tableRow"] = this.showHideDefinition.bind(this);
      this.events['change tr[id*="newEdgeDefinitions"]'] = this.setFromAndTo.bind(this);
      this.events["click .graphViewer-icon-button"] = this.addRemoveDefinition.bind(this);

      return this;
    },

    setFromAndTo : function (e) {
      e.stopPropagation();
      var map = this.calculateEdgeDefinitionMap(), id, i, tmp;

      if (e.added) {
        if (this.eCollList.indexOf(e.added.id) === -1 &&
          this.removedECollList.indexOf(e.added.id) !== -1) {
          id = e.currentTarget.id.split("row_newEdgeDefinitions")[1];
          $('input[id*="newEdgeDefinitions' + id  + '"]').select2("val", null);
          $('input[id*="newEdgeDefinitions' + id  + '"]').attr(
            "placeholder","The collection "+ e.added.id + " is already used."
          );
          return;
        }
        this.removedECollList.push(e.added.id);
        this.eCollList.splice(this.eCollList.indexOf(e.added.id),1);
      } else {
        this.eCollList.push(e.removed.id);
        this.removedECollList.splice(this.removedECollList.indexOf(e.removed.id),1);
      }

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
      /* following not needed? => destroys webif modal
      tmp = $('input[id*="newEdgeDefinitions"]');
      for (i = 0; i < tmp.length ; i++) {
        id = tmp[i].id;
        $('#' + id).select2({
          tags : this.eCollList,
          showSearchBox: false,
          minimumResultsForSearch: -1,
          width: "336px",
          maximumSelectionSize: 1
        });
      }*/
    },

    editGraph : function(e) {
      e.stopPropagation();
      this.collection.fetch();
      this.graphToEdit = this.evaluateGraphName($(e.currentTarget).attr("id"), '_settings');
      var graph = this.collection.findWhere({_key: this.graphToEdit});
      this.createEditGraphModal(
        graph
      );
    },


    saveEditedGraph: function() {
      var name = $("#editGraphName")[0].value,
        editedVertexCollections = _.pluck($('#newVertexCollections').select2("data"), "text"),
        edgeDefinitions = [],
        newEdgeDefinitions = {},
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
            if (from.length !== 0 && to.length !== 0) {
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
      //if no edge definition is left
      if (edgeDefinitions.length === 0) {
        $('#s2id_newEdgeDefinitions0 .select2-choices').css("border-color", "red");
        return;
      }

      //get current edgeDefs/orphanage
      var graph = this.collection.findWhere({_key: name});
      var currentEdgeDefinitions = graph.get("edgeDefinitions");
      var currentOrphanage = graph.get("orphanCollections");
      var currentCollections = [];

      //delete removed orphans
      currentOrphanage.forEach(
        function(oC) {
          if (editedVertexCollections.indexOf(oC) === -1) {
            graph.deleteVertexCollection(oC);
          }
        }
      );
      //add new orphans
      editedVertexCollections.forEach(
        function(vC) {
          if (currentOrphanage.indexOf(vC) === -1) {
            graph.addVertexCollection(vC);
          }
        }
      );

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
      window.modalView.hide();
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

      if (!name) {
        arangoHelper.arangoError(
          "A name for the graph has to be provided."
        );
        return 0;
      }

      if (this.collection.findWhere({_key: name})) {
        arangoHelper.arangoError(
          "The graph '" + name + "' already exists."
        );
        return 0;
      }

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

    createEditGraphModal: function(graph) {
      var buttons = [],
          collList = [],
          tableContent = [],
          collections = this.options.collectionCollection.models,
          self = this,
          name = "",
          edgeDefinitions = [{collection : "", from : "", to :""}],
          orphanCollections = "",
          title;

      this.eCollList = [];
      this.removedECollList = [];

      collections.forEach(function (c) {
        if (c.get("isSystem")) {
          return;
        }
        collList.push(c.id);
        if (c.get('type') === "edge") {
          self.eCollList.push(c.id);
        }
      });
      window.modalView.enableHotKeys = false;
      this.counter = 0;

      if (graph) {
        title = "Edit Graph";

        name = graph.get("_key");
        edgeDefinitions = graph.get("edgeDefinitions");
        if (!edgeDefinitions || edgeDefinitions.length === 0 ) {
          edgeDefinitions = [{collection : "", from : "", to :""}];
        }
        orphanCollections = graph.get("orphanCollections");

        tableContent.push(
          window.modalView.createReadOnlyEntry(
            "editGraphName",
            "Name",
            name,
            "The name to identify the graph. Has to be unique"
          )
        );

        buttons.push(
          window.modalView.createDeleteButton("Delete", this.deleteGraph.bind(this))
        );
        buttons.push(
          window.modalView.createSuccessButton("Save", this.saveEditedGraph.bind(this))
        );

      } else {
        title = "Create Graph";

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

        buttons.push(
          window.modalView.createSuccessButton("Create", this.createNewGraph.bind(this))
        );
      }

      edgeDefinitions.forEach(
        function(edgeDefinition) {
          if (self.counter  === 0) {
            if (edgeDefinition.collection) {
              self.removedECollList.push(edgeDefinition.collection);
              self.eCollList.splice(self.eCollList.indexOf(edgeDefinition.collection),1);
            }
            tableContent.push(
              window.modalView.createSelect2Entry(
                "newEdgeDefinitions" + self.counter,
                "Edge definitions",
                edgeDefinition.collection,
                "An edge definition defines a relation of the graph",
                "Edge definitions",
                true,
                false,
                true,
                1,
                self.eCollList
              )
            );
          } else {
            tableContent.push(
              window.modalView.createSelect2Entry(
                "newEdgeDefinitions" + self.counter,
                "Edge definitions",
                edgeDefinition.collection,
                "An edge definition defines a relation of the graph",
                "Edge definitions",
                false,
                true,
                false,
                1,
                self.eCollList
              )
            );
          }
          tableContent.push(
            window.modalView.createSelect2Entry(
              "fromCollections" + self.counter,
              "fromCollections",
              edgeDefinition.from,
              "The collections that contain the start vertices of the relation.",
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
              "toCollections" + self.counter,
              "toCollections",
              edgeDefinition.to,
              "The collections that contain the end vertices of the relation.",
              "toCollections",
              true,
              false,
              false,
              10,
              collList
            )
          );
          self.counter++;
        }
      );

      tableContent.push(
        window.modalView.createSelect2Entry(
          "newVertexCollections",
          "Vertex collections",
          orphanCollections,
          "Collections that are part of a graph but not used in an edge definition",
          "Vertex Collections",
          false,
          false,
          false,
          10,
          collList
        )
      );

      window.modalView.show(
        "modalGraphTable.ejs", title, buttons, tableContent, null, this.events
      );

      if (graph) {
        var i;
        for (i = 0; i <= this.counter; i++) {
          $('#row_fromCollections' + i).hide();
          $('#row_toCollections' + i).hide();
        }
      }

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
      var collList = [],
        collections = this.options.collectionCollection.models;

      collections.forEach(function (c) {
        if (c.get("isSystem")) {
          return;
        }
        collList.push(c.id);
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
          tags: this.eCollList,
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
    }
  });
}());
