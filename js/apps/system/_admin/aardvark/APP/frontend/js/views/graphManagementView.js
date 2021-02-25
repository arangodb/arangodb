/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, $, _, window, templateEngine, arangoHelper, GraphViewerUI, Joi, frontendConfig */

(function () {
  'use strict';

  window.GraphManagementView = Backbone.View.extend({
    el: '#content',
    template: templateEngine.createTemplate('graphManagementView.ejs'),
    edgeDefintionTemplate: templateEngine.createTemplate('edgeDefinitionTable.ejs'),
    eCollList: [],
    removedECollList: [],
    readOnly: false,

    dropdownVisible: false,

    initialize: function (options) {
      this.options = options;
    },

    events: {
      'click #deleteGraph': 'deleteGraph',
      'click .icon_arangodb_settings2.editGraph': 'editGraph',
      'click #createGraph': 'addNewGraph',
      'keyup #graphManagementSearchInput': 'search',
      'click #graphManagementSearchSubmit': 'search',
      'click .tile-graph': 'redirectToGraphViewer',
      'click #graphManagementToggle': 'toggleGraphDropdown',
      'click .css-label': 'checkBoxes',
      'change #graphSortDesc': 'sorting'
    },

    toggleTab: function (e) {
      var id = e.currentTarget.id;
      id = id.replace('tab-', '');
      $('#tab-content-create-graph .tab-pane').removeClass('active');
      $('#tab-content-create-graph #' + id).addClass('active');

      if (id === 'exampleGraphs') {
        $('#modal-dialog .modal-footer .button-success').css('display', 'none');
      } else {
        $('#modal-dialog .modal-footer .button-success').css('display', 'initial');
      }

      if (id === 'smartGraph') {
        this.setSmartGraphRows(true);
      } else if (id === 'satelliteGraph') {
        this.setSatelliteGraphRows(true);
      } else if (id === 'createGraph') {
        this.setGeneralGraphRows(false);
      }
    },

    // rows that are valid for general, smart & satellite
    generalGraphRows: [
      'row_general-numberOfShards',
      'row_general-replicationFactor',
      'row_general-writeConcern'
    ],

    // rows that needs to be added while creating smarties
    neededSmartGraphRows: [
      'smartGraphInfo',
      'row_new-smartGraphAttribute',
      'row_new-numberOfShards',
      'row_new-replicationFactor',
      'row_new-writeConcern',
      'row_new-isDisjoint'
    ],

    // rows that needs to be hidden while creating satellites
    notNeededSatelliteGraphRows: [
      'row_general-numberOfShards',
      'row_general-replicationFactor',
      'row_general-writeConcern'
    ],

    setGeneralGraphRows: function (cache) {
      this.setCacheModeState(cache);
      this.hideSmartGraphRows();
      _.each(this.generalGraphRows, function (rowId) {
        $('#' + rowId).show();
      });
    },

    setSatelliteGraphRows: function (cache) {
      $('#createGraph').addClass('active');
      this.setCacheModeState(cache);

      this.showGeneralGraphRows();
      this.hideSmartGraphRows();
      _.each(this.notNeededSatelliteGraphRows, function (rowId) {
        $('#' + rowId).hide();
      });
    },

    setSmartGraphRows: function (cache) {
      $('#createGraph').addClass('active');
      this.setCacheModeState(cache);

      this.hideGeneralGraphRows();
      _.each(this.neededSmartGraphRows, function (rowId) {
        $('#' + rowId).show();
      });
    },

    hideSmartGraphRows: function () {
      _.each(this.neededSmartGraphRows, function (rowId) {
        $('#' + rowId).hide();
      });
    },

    showGeneralGraphRows: function () {
      _.each(this.generalGraphRows, function (rowId) {
        $('#' + rowId).show();
      });
    },

    hideGeneralGraphRows: function () {
      _.each(this.generalGraphRows, function (rowId) {
        $('#' + rowId).hide();
      });
    },

    redirectToGraphViewer: function (e) {
      var name = $(e.currentTarget).attr('id');
      if (name) {
        name = name.substr(0, name.length - 5);
        window.App.navigate('graph/' + encodeURIComponent(name), {trigger: true});
      }
    },

    loadGraphViewer: function (graphName, refetch) {
      var callback = function (error) {
        if (error) {
          arangoHelper.arangoError('', '');
        } else {
          var edgeDefs = this.collection.get(graphName).get('edgeDefinitions');
          if (!edgeDefs || edgeDefs.length === 0) {
            // User Info
            return;
          }
          var adapterConfig = {
            type: 'gharial',
            graphName: graphName,
            baseUrl: arangoHelper.databaseUrl('/')
          };
          var width = $('#content').width() - 75;
          $('#content').html('');

          var height = arangoHelper.calculateCenterDivHeight();

          this.ui = new GraphViewerUI($('#content')[0], adapterConfig, width, $('.centralRow').height() - 135, {
            nodeShaper: {
              label: '_key',
              color: {
                type: 'attribute',
                key: '_key'
              }
            }

          }, true);

          $('.contentDiv').height(height);
        }
      }.bind(this);

      if (refetch) {
        this.collection.fetch({
          cache: false,
          success: function () {
            callback();
          }
        });
      } else {
        callback();
      }
    },

    handleResize: function (w) {
      if (!this.width || this.width !== w) {
        this.width = w;
        if (this.ui) {
          this.ui.changeWidth(w);
        }
      }
    },

    addNewGraph: function (e) {
      e.preventDefault();
      if (!this.readOnly) {
        if (frontendConfig.isCluster && frontendConfig.isEnterprise) {
          this.createEditGraphModal();
        } else {
          this.createEditGraphModal();
          // hide tab entries
          // no SmartGraphs in single server mode
          $('#tab-smartGraph').parent().remove();
          // no SatelliteGraphs in single server mode
          $('#tab-satelliteGraph').parent().remove();
        }
      }
    },

    deleteGraph: function () {
      var self = this;
      var name = $('#editGraphName')[0].value;

      if ($('#dropGraphCollections').is(':checked')) {
        var callback = function (success, data) {
          window.modalView.hide();
          if (success) {
            self.collection.remove(self.collection.get(name));
          } else {
            if (data && data.error && data.errorMessage) {
              arangoHelper.arangoError('Graph', data.errorMessage);
            } else {
              arangoHelper.arangoError('Graph', 'Could not delete Graph.');
            }
          }
          // trigger in success and error case
          // e.g. graph deletion might work, but e.g. some collections could not be dropped (distributeShardsLike)
          self.updateGraphManagementView();
        };

        this.collection.dropAndDeleteGraph(name, callback);
      } else {
        this.collection.get(name).destroy({
          success: function () {
            self.updateGraphManagementView();
            window.modalView.hide();
          },
          error: function (xhr, err) {
            var response = JSON.parse(err.responseText);
            var msg = response.errorMessage;
            arangoHelper.arangoError(msg);
            window.modalView.hide();
          }
        });
      }
    },

    checkBoxes: function (e) {
      // chrome bugfix
      var clicked = e.currentTarget.id;
      $('#' + clicked).click();
    },

    checkVisibility: function () {
      if ($('#graphManagementDropdown').is(':visible')) {
        this.dropdownVisible = true;
      } else {
        this.dropdownVisible = false;
      }
      arangoHelper.setCheckboxStatus('#graphManagementDropdown');
    },

    toggleGraphDropdown: function () {
      var self = this;
      // apply sorting to checkboxes
      $('#graphSortDesc').attr('checked', this.collection.sortOptions.desc);

      $('#graphManagementToggle').toggleClass('activated');
      $('#graphManagementDropdown2').slideToggle(200, function () {
        self.checkVisibility();
      });
    },

    sorting: function () {
      if ($('#graphSortDesc').is(':checked')) {
        this.collection.setSortingDesc(true);
      } else {
        this.collection.setSortingDesc(false);
      }

      this.checkVisibility();
      this.render();
    },

    createExampleGraphs: function (e) {
      var graph = $(e.currentTarget).attr('graph-id');
      var self = this;

      $.ajax({
        type: 'POST',
        url: arangoHelper.databaseUrl('/_admin/aardvark/graph-examples/create/' + encodeURIComponent(graph)),
        success: function () {
          window.modalView.hide();
          self.updateGraphManagementView();
          arangoHelper.arangoNotification('Example Graphs', 'Graph: ' + graph + ' created.');
        },
        error: function (err) {
          window.modalView.hide();
          if (err.responseText) {
            try {
              var msg = JSON.parse(err.responseText);
              arangoHelper.arangoError('Example Graphs', msg.errorMessage);
            } catch (e) {
              arangoHelper.arangoError('Example Graphs', 'Could not create example graph: ' + graph);
            }
          } else {
            arangoHelper.arangoError('Example Graphs', 'Could not create example graph: ' + graph);
          }
        }
      });
    },

    forgetCachedCollectionsState: function () {
      // Note: re-enable cached collections for general graph
      // General graph collections are allowed to use existing collections
      // SatelliteGraphs and SmartGraphs are not allowed to use them, so we need to "forget" them here
      var collList = [];
      var self = this;
      var collections = this.options.collectionCollection.models;

      collections.forEach(function (c) {
        if (c.get('isSystem')) {
          return;
        }
        collList.push(c.id);
      });

      var i;
      for (i = 0; i < this.counter; i++) {
        $('#newEdgeDefinitions' + i).select2({
          tags: self.eCollList
        });
        $('#newEdgeDefinitions' + i).select2('data', self.cachedNewEdgeDefinitions);
        $('#newEdgeDefinitions' + i).attr('disabled', self.cachedNewEdgeDefinitionsState);

        $('#fromCollections' + i).select2({
          tags: collList
        });
        $('#fromCollections' + i).select2('data', self.cachedFromCollections);
        $('#fromCollections' + i).attr('disabled', self.cachedFromCollectionsState);

        $('#toCollections' + i).select2({
          tags: collList
        });
        $('#toCollections' + i).select2('data', self.cachedToCollections);
        $('#toCollections' + i).attr('disabled', self.cachedToCollectionsState);
      }
      $('#newVertexCollections').select2({
        tags: collList
      });
      $('#newVertexCollections').select2('data', self.cachedNewVertexCollections);
      $('#newVertexCollections').attr('disabled', self.cachedNewVertexCollectionsState);
    },

    rememberCachedCollectionsState: function () {
      var self = this;
      var i;

      for (i = 0; i < self.counter; i++) {
        $('#newEdgeDefinitions' + i).select2({
          tags: []
        });
        self.cachedNewEdgeDefinitions = $('#newEdgeDefinitions' + i).select2('data');
        self.cachedNewEdgeDefinitionsState = $('#newEdgeDefinitions' + i).attr('disabled');
        $('#newEdgeDefinitions' + i).select2('data', '');
        $('#newEdgeDefinitions' + i).attr('disabled', false);
        $('#newEdgeDefinitions' + i).change();

        $('#fromCollections' + i).select2({
          tags: []
        });
        self.cachedFromCollections = $('#fromCollections' + i).select2('data');
        self.cachedFromCollectionsState = $('#fromCollections' + i).attr('disabled');
        $('#fromCollections' + i).select2('data', '');
        $('#fromCollections' + i).attr('disabled', false);
        $('#fromCollections' + i).change();

        $('#toCollections' + i).select2({
          tags: []
        });
        self.cachedToCollections = $('#toCollections' + i).select2('data');
        self.cachedToCollectionsState = $('#toCollections' + i).attr('disabled');
        $('#toCollections' + i).select2('data', '');
        $('#toCollections' + i).attr('disabled', false);
        $('#toCollections' + i).change();
      }
      $('#newVertexCollections').select2({
        tags: []
      });
      self.cachedNewVertexCollections = $('#newVertexCollections').select2('data');
      self.cachedNewVertexCollectionsState = $('#newVertexCollections').attr('disabled');
      $('#newVertexCollections').select2('data', '');
      $('#newVertexCollections').attr('disabled', false);
      $('#newVertexCollections').change();
    },

    setCacheModeState: function (forget) {
      if (!frontendConfig.isCluster || !frontendConfig.isEnterprise) {
        return;
      }

      if (forget) {
        this.forgetCachedCollectionsState();
      } else {
        this.rememberCachedCollectionsState();
      }
    },

    render: function (name, refetch) {
      var self = this;
      this.collection.fetch({
        cache: false,

        success: function () {
          self.collection.sort();

          $(self.el).html(self.template.render({
            graphs: self.collection,
            searchString: ''
          }));

          if (self.dropdownVisible === true) {
            $('#graphManagementDropdown2').show();
            $('#graphSortDesc').attr('checked', self.collection.sortOptions.desc);
            $('#graphManagementToggle').toggleClass('activated');
            $('#graphManagementDropdown').show();
          }

          self.events['change tr[id*="newEdgeDefinitions"]'] = self.setFromAndTo.bind(self);
          self.events['click .graphViewer-icon-button'] = self.addRemoveDefinition.bind(self);
          self.events['click #graphTab a'] = self.toggleTab.bind(self);
          self.events['click .createExampleGraphs'] = self.createExampleGraphs.bind(self);
          self.events['focusout .select2-search-field input'] = function (e) {
            if ($('.select2-drop').is(':visible')) {
              if (!$('#select2-search-field input').is(':focus')) {
                window.setTimeout(function () {
                  $(e.currentTarget).parent().parent().parent().select2('close');
                }, 200);
              }
            }
          };
          arangoHelper.setCheckboxStatus('#graphManagementDropdown');
          arangoHelper.checkDatabasePermissions(self.setReadOnly.bind(self));
        }
      });

      if (name) {
        this.loadGraphViewer(name, refetch);
      }
      return this;
    },

    setReadOnly: function () {
      this.readOnly = true;
      $('#createGraph').parent().parent().addClass('disabled');
      $('#createGraph').addClass('disabled');
    },

    setFromAndTo: function (e) {
      e.stopPropagation();
      var map = this.calculateEdgeDefinitionMap();
      var id;

      if ($('#tab-smartGraph').parent().hasClass('active')) {
        if (e.added) {
          if (this.eCollList.indexOf(e.added.id) === -1 && this.removedECollList.indexOf(e.added.id) !== -1) {
            id = e.currentTarget.id.split('row_newEdgeDefinitions')[1];
            $('input[id*="newEdgeDefinitions' + id + '"]').select2('val', null);
            $('input[id*="newEdgeDefinitions' + id + '"]').attr(
              'placeholder', 'The collection ' + e.added.id + ' is already used.'
            );
            return;
          }
          this.removedECollList.push(e.added.id);
          this.eCollList.splice(this.eCollList.indexOf(e.added.id), 1);
        } else {
          if (e.removed) {
            // TODO edges not properly removed within selection
            this.eCollList.push(e.removed.id);
            this.removedECollList.splice(this.removedECollList.indexOf(e.removed.id), 1);
          }
        }
        if (map[e.val]) {
          id = e.currentTarget.id.split('row_newEdgeDefinitions')[1];
          $('#s2id_fromCollections' + id).select2('val', map[e.val].from);
          $('#fromCollections' + id).attr('disabled', true);
          $('#s2id_toCollections' + id).select2('val', map[e.val].to);
          $('#toCollections' + id).attr('disabled', true);
        } else {
          id = e.currentTarget.id.split('row_newEdgeDefinitions')[1];
          $('#s2id_fromCollections' + id).select2('val', null);
          $('#fromCollections' + id).attr('disabled', false);
          $('#s2id_toCollections' + id).select2('val', null);
          $('#toCollections' + id).attr('disabled', false);
        }
      }
    },

    editGraph: function (e) {
      e.stopPropagation();
      this.collection.fetch({
        cache: false
      });
      this.graphToEdit = this.evaluateGraphName($(e.currentTarget).attr('id'), '_settings');
      var graph = this.collection.findWhere({_key: this.graphToEdit});
      if (graph.get('isSmart')) {
        this.createEditGraphModal(graph, true);
      } else if (graph.get('replicationFactor') === 'satellite') {
        this.createEditGraphModal(graph, false, true);
      } else {
        this.createEditGraphModal(graph);
      }
    },

    saveEditedGraph: function () {
      var name = $('#editGraphName')[0].value;
      var editedVertexCollections = _.pluck($('#newVertexCollections').select2('data'), 'text');
      var edgeDefinitions = [];
      var newEdgeDefinitions = {};
      var collection;
      var from;
      var to;
      var index;
      var edgeDefinitionElements;

      edgeDefinitionElements = $('[id^=s2id_newEdgeDefinitions]').toArray();
      edgeDefinitionElements.forEach(
        function (eDElement) {
          index = $(eDElement).attr('id');
          index = index.replace('s2id_newEdgeDefinitions', '');
          collection = _.pluck($('#s2id_newEdgeDefinitions' + index).select2('data'), 'text')[0];
          if (collection && collection !== '') {
            from = _.pluck($('#s2id_fromCollections' + index).select2('data'), 'text');
            to = _.pluck($('#s2id_toCollections' + index).select2('data'), 'text');
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

      // if no edge definition is left
      if (edgeDefinitions.length === 0) {
        $('#s2id_newEdgeDefinitions0 .select2-choices').css('border-color', 'red');
        $('#s2id_newEdgeDefinitions0')
          .parent()
          .parent()
          .next().find('.select2-choices').css('border-color', 'red');
        $('#s2id_newEdgeDefinitions0').parent()
          .parent()
          .next()
          .next()
          .find('.select2-choices')
          .css('border-color', 'red');
        return;
      }

      // get current edgeDefs/orphanage
      var graph = this.collection.findWhere({_key: name});
      var currentEdgeDefinitions = graph.get('edgeDefinitions');
      var currentOrphanage = graph.get('orphanCollections');
      var currentCollections = [];

      // delete removed orphans
      currentOrphanage.forEach(
        function (oC) {
          if (editedVertexCollections.indexOf(oC) === -1) {
            graph.deleteVertexCollection(oC);
          }
        }
      );
      // add new orphans
      editedVertexCollections.forEach(
        function (vC) {
          if (currentOrphanage.indexOf(vC) === -1) {
            graph.addVertexCollection(vC);
          }
        }
      );

      // evaluate all new, edited and deleted edge definitions
      var newEDs = [];
      var editedEDs = [];
      var deletedEDs = [];

      currentEdgeDefinitions.forEach(
        function (eD) {
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
        function (eD) {
          var collection = eD.collection;
          if (currentCollections.indexOf(collection) === -1) {
            newEDs.push(collection);
          }
        }
      );

      newEDs.forEach(
        function (eD) {
          graph.addEdgeDefinition(newEdgeDefinitions[eD]);
        }
      );

      editedEDs.forEach(
        function (eD) {
          graph.modifyEdgeDefinition(newEdgeDefinitions[eD]);
        }
      );

      deletedEDs.forEach(
        function (eD) {
          graph.deleteEdgeDefinition(eD);
        }
      );
      this.updateGraphManagementView();
      window.modalView.hide();
    },

    evaluateGraphName: function (str, substr) {
      var index = str.lastIndexOf(substr);
      return str.substring(0, index);
    },

    search: function () {
      var searchInput,
        searchString,
        strLength,
        reducedCollection;

      searchInput = $('#graphManagementSearchInput');
      searchString = arangoHelper.escapeHtml($('#graphManagementSearchInput').val());
      reducedCollection = this.collection.filter(
        function (u) {
          return u.get('_key').indexOf(searchString) !== -1;
        }
      );
      $(this.el).html(this.template.render({
        graphs: reducedCollection,
        searchString: searchString
      }));

      // after rendering, get the "new" element
      searchInput = $('#graphManagementSearchInput');
      // set focus on end of text in input field
      strLength = searchInput.val().length;
      searchInput.focus();
      searchInput[0].setSelectionRange(strLength, strLength);
    },

    updateGraphManagementView: function () {
      var self = this;
      this.collection.fetch({
        cache: false,
        success: function () {
          self.render();
        }
      });
    },

    createNewGraph: function () {
      var name = $('#createNewGraphName').val();
      var vertexCollections = _.pluck($('#newVertexCollections').select2('data'), 'text');
      var edgeDefinitions = [];
      var self = this;
      var collection;
      var from;
      var to;
      var index;
      var edgeDefinitionElements;

      if (!name) {
        arangoHelper.arangoError(
          'A name for the graph has to be provided.'
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
        function (eDElement) {
          index = $(eDElement).attr('id');
          index = index.replace('s2id_newEdgeDefinitions', '');
          collection = _.pluck($('#s2id_newEdgeDefinitions' + index).select2('data'), 'text')[0];
          if (collection && collection !== '') {
            from = _.pluck($('#s2id_fromCollections' + index).select2('data'), 'text');
            to = _.pluck($('#s2id_toCollections' + index).select2('data'), 'text');
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

      if (edgeDefinitions.length === 0) {
        $('#s2id_newEdgeDefinitions0 .select2-choices').css('border-color', 'red');
        $('#s2id_newEdgeDefinitions0').parent()
          .parent()
          .next()
          .find('.select2-choices')
          .css('border-color', 'red');
        $('#s2id_newEdgeDefinitions0').parent()
          .parent()
          .next()
          .next()
          .find('.select2-choices')
          .css('border-color', 'red');
        return;
      }

      var newCollectionObject = {
        name: name,
        edgeDefinitions: edgeDefinitions,
        orphanCollections: vertexCollections
      };

      // if SmartGraph
      if ($('#tab-smartGraph').parent().hasClass('active')) {
        if ($('#new-numberOfShards').val() === '' || $('#new-smartGraphAttribute').val() === '') {
          arangoHelper.arangoError('SmartGraph creation', 'numberOfShards and/or smartGraphAttribute not set!');
          return;
        } else {
          newCollectionObject.isSmart = true;
          newCollectionObject.options = {
            numberOfShards: parseInt($('#new-numberOfShards').val()),
            smartGraphAttribute: $('#new-smartGraphAttribute').val(),
            replicationFactor: parseInt($('#new-replicationFactor').val()),
            minReplicationFactor: parseInt($('#new-writeConcern').val()),
            isDisjoint: $('#new-isDisjoint').is(':checked')
          };
        }
      } else if ($('#tab-satelliteGraph').parent().hasClass('active')) {
        newCollectionObject.options = {
          replicationFactor: "satellite"
        };
      } else {
        if (frontendConfig.isCluster) {
          if ($('#general-numberOfShards').val().length > 0) {
            newCollectionObject.options = {
              numberOfShards: parseInt($('#general-numberOfShards').val())
            };
          }
          if ($('#general-replicationFactor').val().length > 0) {
            if (newCollectionObject.options) {
              newCollectionObject.options.replicationFactor = parseInt($('#general-replicationFactor').val());
            } else {
              newCollectionObject.options = {
                replicationFactor: parseInt($('#general-replicationFactor').val())
              };
            }
          }
          if ($('#general-writeConcern').val().length > 0) {
            if (newCollectionObject.options) {
              newCollectionObject.options.minReplicationFactor = parseInt($('#general-writeConcern').val());
            } else {
              newCollectionObject.options = {
                minReplicationFactor: parseInt($('#general-writeConcern').val())
              };
            }
          }
        }
      }

      this.collection.create(newCollectionObject, {
        success: function () {
          self.updateGraphManagementView();
          window.modalView.hide();
        },
        error: function (obj, err) {
          var response = JSON.parse(err.responseText);
          var msg = response.errorMessage;
          // Gritter does not display <>
          msg = msg.replace('<', '');
          msg = msg.replace('>', '');
          arangoHelper.arangoError(msg);
        }
      });
    },

    createEditGraphModal: function (graph, isSmart, isSatellite) {
      var buttons = [];
      var collList = [];
      var tableContent = [];
      var collections = this.options.collectionCollection.models;
      var self = this;
      var name = '';
      var edgeDefinitions = [{collection: '', from: '', to: ''}];
      var orphanCollections = '';
      var title;
      var sorter = function (l, r) {
        l = l.toLowerCase();
        r = r.toLowerCase();
        if (l < r) {
          return -1;
        }
        if (l > r) {
          return 1;
        }
        return 0;
      };

      this.eCollList = [];
      this.removedECollList = [];

      collections.forEach(function (c) {
        if (c.get('isSystem')) {
          return;
        }
        if (c.get('type') === 'edge') {
          self.eCollList.push(c.id);
        } else {
          collList.push(c.id);
        }
      });
      this.counter = 0;

      // edit graph section
      if (graph) {
        if (isSmart) {
          title = 'Edit SmartGraph';
        } else if (isSatellite) {
          title = 'Edit SatelliteGraph';
        } else {
          title = 'Edit Graph';
        }

        name = graph.get('_key');
        edgeDefinitions = graph.get('edgeDefinitions');
        if (!edgeDefinitions || edgeDefinitions.length === 0) {
          edgeDefinitions = [{collection: '', from: '', to: ''}];
        }
        orphanCollections = graph.get('orphanCollections');

        tableContent.push(
          window.modalView.createReadOnlyEntry(
            'editGraphName',
            'Name',
            name,
            'The name to identify the graph. Has to be unique'
          )
        );

        if (isSmart) {
          tableContent.push(
            window.modalView.createReadOnlyEntry(
              'smartGraphAttribute',
              'SmartGraph Attribute',
              graph.get('smartGraphAttribute'),
              'The attribute name that is used to smartly shard the vertices of a graph. \n' +
              'Every vertex in this Graph has to have this attribute. \n'
            )
          );
        }

        if (graph.get('numberOfShards')) {
          tableContent.push(
            window.modalView.createReadOnlyEntry(
              'numberOfShards',
              'Shards',
              graph.get('numberOfShards'),
              'Number of shards the graph is using.'
            )
          );
        }

        if (graph.get('replicationFactor')) {
          tableContent.push(
            window.modalView.createReadOnlyEntry(
              'replicationFactor',
              'Replication factor',
              graph.get('replicationFactor'),
              'Total number of desired copies of the data in the cluster.'
            )
          );
        }

        if (graph.get('minReplicationFactor')) {
          tableContent.push(
            window.modalView.createReadOnlyEntry(
              'writeConcern',
              'Write concern',
              graph.get('minReplicationFactor'),
              'Numeric value. Must be at least 1. Must be smaller or equal compared to the replication factor. Total number of copies of the data in the cluster that are required for each write operation. If we get below this value the collection will be read-only until enough copies are created.'
            )
          );
        }

        if (isSmart) {
          let isDisjoint = 'No';
          if (graph.get('isDisjoint')) {
            isDisjoint = 'Yes';
          }
          tableContent.push(
            window.modalView.createReadOnlyEntry(
              'isDisjoint',
              'Disjoint SmartGraph',
              isDisjoint,
              'Disjoint SmartGraph: Creating edges between different SmartGraph components is not allowed.',
            )
          );
        }

        buttons.push(
          window.modalView.createDeleteButton('Delete', this.deleteGraph.bind(this))
        );
        buttons.push(
          window.modalView.createNotificationButton('Reset display settings', this.resetDisplaySettings.bind(this))
        );
        buttons.push(
          window.modalView.createSuccessButton('Save', this.saveEditedGraph.bind(this))
        );
      } else {
        // create graph section
        title = 'Create Graph';

        tableContent.push(
          window.modalView.createTextEntry(
            'createNewGraphName',
            'Name',
            '',
            'The name to identify the graph. Has to be unique.',
            'graphName',
            true
          )
        );

        buttons.push(
          window.modalView.createSuccessButton('Create', this.createNewGraph.bind(this))
        );
      }

      if (frontendConfig.isEnterprise === true && frontendConfig.isCluster && !graph) {
        tableContent.push(
          window.modalView.createTextEntry(
            'new-numberOfShards',
            'Shards*',
            '',
            'Number of shards the SmartGraph is using.',
            '',
            false,
            [
              {
                rule: Joi.string().allow('').optional().regex(/^[0-9]*$/),
                msg: 'Must be a number.'
              }
            ]
          )
        );

        tableContent.push(
          window.modalView.createTextEntry(
            'new-replicationFactor',
            'Replication factor',
            '',
            'Numeric value. Must be at least 1. Total number of copies of the data in the cluster.',
            '',
            false,
            [
              {
                rule: Joi.string().allow('').optional().regex(/^[1-9]*$/),
                msg: 'Must be a number.'
              }
            ]
          )
        );

        tableContent.push(
          window.modalView.createTextEntry(
            'new-writeConcern',
            'Write concern',
            '',
            'Numeric value. Must be at least 1. Must be smaller or equal compared to the replication factor. Total number of copies of the data in the cluster that are required for each write operation. If we get below this value the collection will be read-only until enough copies are created.',
            '',
            false,
            [
              {
                rule: Joi.string().allow('').optional().regex(/^[1-9]*$/),
                msg: 'Numeric value. Must be at least 1. Must be smaller or equal compared to the replication factor. Total number of copies of the data in the cluster that are required for each write operation. If we get below this value the collection will be read-only until enough copies are created.'
              }
            ]
          )
        );

        tableContent.push(
          window.modalView.createCheckboxEntry(
            'new-isDisjoint',
            'Create disjoint graph',
            false,
            'Creates a Disjoint SmartGraph. Creating edges between different SmartGraph components is not allowed.',
          )
        );

        tableContent.push(
          window.modalView.createTextEntry(
            'new-smartGraphAttribute',
            'SmartGraph Attribute*',
            '',
            'The attribute name that is used to smartly shard the vertices of a graph. \n' +
            'Every vertex in this Graph has to have this attribute. \n' +
            'Cannot be modified later.',
            '',
            false,
            [
              {
                rule: Joi.string().allow('').optional(),
                msg: 'Must be a string.'
              }
            ]
          )
        );
      }

      if (frontendConfig.isCluster && !graph) {
        tableContent.push(
          window.modalView.createTextEntry(
            'general-numberOfShards',
            'Shards',
            '',
            'Number of shards the graph is using.',
            '',
            false,
            [
              {
                rule: Joi.string().allow('').optional().regex(/^[0-9]*$/),
                msg: 'Must be a number.'
              }
            ]
          )
        );
        tableContent.push(
          window.modalView.createTextEntry(
            'general-replicationFactor',
            'Replication factor',
            '',
            'Numeric value. Must be at least 1. Total number of desired copies of the data in the cluster.',
            '',
            false,
            [
              {
                rule: Joi.string().allow('').optional().regex(/^[1-9]*$/),
                msg: 'Must be a number.'
              }
            ]
          )
        );
        tableContent.push(
          window.modalView.createTextEntry(
            'general-writeConcern',
            'Write concern',
            '',
            'Numeric value. Must be at least 1. Must be smaller or equal compared to the replication factor. Total number of copies of the data in the cluster that are required for each write operation. If we get below this value the collection will be read-only until enough copies are created.',
            '',
            false,
            [
              {
                rule: Joi.string().allow('').optional().regex(/^[1-9]*$/),
                msg: 'Must be a number. Must be at least 1 and has to be smaller or equal compared to the replicationFactor.'
              }
            ]
          )
        );
      }

      edgeDefinitions.forEach(
        function (edgeDefinition) {
          if (self.counter === 0) {
            if (edgeDefinition.collection) {
              self.removedECollList.push(edgeDefinition.collection);
              self.eCollList.splice(self.eCollList.indexOf(edgeDefinition.collection), 1);
            }
            tableContent.push(
              window.modalView.createSelect2Entry(
                'newEdgeDefinitions' + self.counter,
                'Edge definitions',
                edgeDefinition.collection,
                'An edge definition defines a relation of the graph',
                'Edge definitions',
                true,
                false,
                true,
                1,
                self.eCollList.sort(sorter)
              )
            );
          } else {
            tableContent.push(
              window.modalView.createSelect2Entry(
                'newEdgeDefinitions' + self.counter,
                'Edge definitions',
                edgeDefinition.collection,
                'An edge definition defines a relation of the graph',
                'Edge definitions',
                false,
                true,
                false,
                1,
                self.eCollList.sort(sorter)
              )
            );
          }
          tableContent.push(
            window.modalView.createSelect2Entry(
              'fromCollections' + self.counter,
              'fromCollections',
              edgeDefinition.from,
              'The collections that contain the start vertices of the relation.',
              'fromCollections',
              true,
              false,
              false,
              null,
              collList.sort(sorter)
            )
          );
          tableContent.push(
            window.modalView.createSelect2Entry(
              'toCollections' + self.counter,
              'toCollections',
              edgeDefinition.to,
              'The collections that contain the end vertices of the relation.',
              'toCollections',
              true,
              false,
              false,
              null,
              collList.sort(sorter)
            )
          );
          self.counter++;
        }
      );

      tableContent.push(
        window.modalView.createSelect2Entry(
          'newVertexCollections',
          'Vertex collections',
          orphanCollections,
          'Collections that are part of a graph but not used in an edge definition',
          'Vertex Collections',
          false,
          false,
          false,
          null,
          collList.sort(sorter)
        )
      );

      window.modalView.show(
        'modalGraphTable.ejs', title, buttons, tableContent, undefined, undefined, this.events
      );

      if ($('#tab-createGraph').parent().hasClass('active')) {
        // hide them by default, as we're showing general graph as default
        // satellite does not need to appear here as it has no additional input fields
        self.hideSmartGraphRows();
      }

      if (graph) {
        $('.modal-body table').css('border-collapse', 'separate');
        var i;

        $('.modal-body .spacer').remove();
        for (i = 0; i <= this.counter; i++) {
          $('#row_fromCollections' + i).show();
          $('#row_toCollections' + i).show();
          $('#row_newEdgeDefinitions' + i).addClass('first');
          $('#row_fromCollections' + i).addClass('middle');
          $('#row_toCollections' + i).addClass('last');
          $('#row_toCollections' + i).after('<tr id="spacer' + i + '" class="spacer"></tr>');
        }

        $('#graphTab').hide();
        $('#modal-dialog .modal-delete-confirmation').append(
          '<fieldset><input type="checkbox" id="dropGraphCollections" name="" value="">' +
          '<label for="dropGraphCollections">also drop collections?</label>' +
          '</fieldset>'
        );
      }
    },

    resetDisplaySettings: function () {
      var graphName = $('#editGraphName').val();

      var test = new window.GraphSettingsView({
        name: graphName,
        userConfig: window.App.userConfig
      });
      test.setDefaults(true, true);
      test.remove();

      window.modalView.hide();
      arangoHelper.arangoNotification('Graph', 'Reset successful.');
    },

    addRemoveDefinition: function (e) {
      var collList = [];
      var collections = this.options.collectionCollection.models;

      collections.forEach(function (c) {
        if (!c.get('isSystem')) {
          if (c.get('type') !== 'edge') {
            collList.push(c.id);
          }
        }
      });
      e.stopPropagation();
      var id = $(e.currentTarget).attr('id');
      var number;
      if (id.indexOf('addAfter_newEdgeDefinitions') !== -1) {
        this.counter++;
        $('#row_newVertexCollections').before(
          this.edgeDefintionTemplate.render({
            number: this.counter
          })
        );
        $('#newEdgeDefinitions' + this.counter).select2({
          tags: this.eCollList,
          showSearchBox: false,
          minimumResultsForSearch: -1,
          width: '336px',
          maximumSelectionSize: 1
        });
        $('#fromCollections' + this.counter).select2({
          tags: collList,
          showSearchBox: false,
          minimumResultsForSearch: -1,
          width: '336px'
          // maximumSelectionSize: 10
        });
        $('#toCollections' + this.counter).select2({
          tags: collList,
          showSearchBox: false,
          minimumResultsForSearch: -1,
          width: '336px'
          // maximumSelectionSize: 10
        });
        window.modalView.undelegateEvents();
        window.modalView.delegateEvents(this.events);

        arangoHelper.fixTooltips('.icon_arangodb, .arangoicon', 'right');

        var i;
        $('.modal-body .spacer').remove();
        for (i = 0; i <= this.counter; i++) {
          $('#row_fromCollections' + i).show();
          $('#row_toCollections' + i).show();
          $('#row_newEdgeDefinitions' + i).addClass('first');
          $('#row_fromCollections' + i).addClass('middle');
          $('#row_toCollections' + i).addClass('last');
          $('#row_toCollections' + i).after('<tr id="spacer' + i + '" class="spacer"></tr>');
        }
        return;
      }
      if (id.indexOf('remove_newEdgeDefinitions') !== -1) {
        number = id.split('remove_newEdgeDefinitions')[1];
        $('#row_newEdgeDefinitions' + number).remove();
        $('#row_fromCollections' + number).remove();
        $('#row_toCollections' + number).remove();
        $('#spacer' + number).remove();
      }
    },

    calculateEdgeDefinitionMap: function () {
      var edgeDefinitionMap = {};
      this.collection.models.forEach(function (m) {
        m.get('edgeDefinitions').forEach(function (ed) {
          edgeDefinitionMap[ed.collection] = {
            from: ed.from,
            to: ed.to
          };
        });
      });
      return edgeDefinitionMap;
    }
  });
}());
