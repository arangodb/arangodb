/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, $, _, window, templateEngine, arangoHelper, GraphViewerUI, Joi, frontendConfig */

(function () {
  'use strict';

  window.GraphManagementView = Backbone.View.extend({
    el: '#content',
    template: templateEngine.createTemplate('graphManagementView.ejs'),
    edgeDefintionTemplate: templateEngine.createTemplate('edgeDefinitionTable.ejs'),
    readOnly: false,
    smartGraphName: "SmartGraph",
    enterpriseGraphName: "EnterpriseGraph",
    currentGraphCreationType: (frontendConfig.isEnterprise ? 'enterprise' : 'community'),
    currentGraphEditType: null,

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

      if (id === 'smartGraph' || id === 'enterpriseGraph') {
        if (id === 'smartGraph') {
          this.currentGraphCreationType = 'smart';
        } else {
          this.currentGraphCreationType = 'enterprise';
        }
        this.setSmartGraphRows(true, id);
      } else if (id === 'satelliteGraph') {
        this.currentGraphCreationType = 'satellite';
        this.setSatelliteGraphRows(true);
      } else if (id === 'createGraph') {
        this.currentGraphCreationType = 'community';
        this.setGeneralGraphRows(false);
      }
    },

    // rows that are valid for general, smart & satellite
    generalGraphRows: [
      'row_general-numberOfShards',
      'row_general-replicationFactor',
      'row_general-writeConcern'
    ],

    potentiallyNeededSmartGraphRows: [
      'smartGraphInfoOneShard'
    ],

    // rows that needs to be added while creating smarties
    neededSmartGraphRows: [
      'smartGraphInfo',
      'row_new-smartGraphAttribute',
      'row_new-numberOfShards',
      'row_new-replicationFactor',
      'row_new-writeConcern',
      'row_hybridSatelliteCollections',
      'row_new-isDisjoint'
    ],

    nonNeededEnterpriseGraphRows: [
      'row_new-smartGraphAttribute',
      'row_new-isDisjoint'
    ],

    // rows that needs to be hidden while creating satellites
    notNeededSatelliteGraphRows: [
      'row_general-numberOfShards',
      'row_general-replicationFactor',
      'row_hybridSatelliteCollections',
      'row_general-writeConcern'
    ],

    getCurrentCreationGraphType: function () {
      if (this.currentGraphCreationType) {
        return this.currentGraphCreationType;
      }

      // fallback - should not be required...
      if ($('#tab-smartGraph').parent().hasClass('active')) {
        return 'smart';
      } else if ($('#tab-enterpriseGraph').parent().hasClass('active')) {
        return 'enterprise';
      } else if ($('#tab-satelliteGraph').parent().hasClass('active')) {
        return 'satellite';
      } else if ($('#tab-createGraph').parent().hasClass('active')) {
        return 'community';
      } else {
        return 'none';
      }
    },

    isEnterpriseOnlyGraphOrIsDefaultIsEnterprise: function (editType) {
      if (editType && editType === 'community') {
        return false;
      }

      const graphCreationType = editType || this.getCurrentCreationGraphType();
      if (this.isEnterpriseOnlyGraph(graphCreationType)) {
        return true;
      }

      if (graphCreationType === 'none') {
        // In this special case we'll deliver the default
        if (frontendConfig.isEnterprise) {
          return true;
        }
      }

      return false;
    },

    isEnterpriseOnlyGraph: function (graphCreationType) {
      if (graphCreationType === 'smart' || graphCreationType === 'enterprise' || graphCreationType === 'satellite') {
        return true;
      }
      return false;
    },

    buildAutoCompletionLists: function (type, editGraphType) {
      // Supported type is either: "edges" or "vertices"

      let collections = [];

      const graphCreationType = editGraphType || this.getCurrentCreationGraphType();
      if (this.isEnterpriseOnlyGraph(graphCreationType)) {
        // In that case, all collections need to be newly created. Therefore, we cannot provide any
        // auto-completion feature.
        return collections;
      } else if (graphCreationType === 'none') {
        // Means we're in the init phase, and the modal view has not been rendered right now.
        // In this case, we need to decide whether we want to have autocomplete on or off
        // related to the environment we're running at. If we do have an Enterprise instance,
        // by default it will be off (as EnterpriseGraphs are the default and need new collections).
        // In Community edition, it will be enabled by default.
        if (frontendConfig.isEnterprise) {
          return collections;
        }
      }

      // Will re-build the local state every time, as we might have newly created collections meanwhile
      const storedCollections = this.options.collectionCollection.models;
      storedCollections.forEach(function (c) {
        if (c.get('isSystem')) {
          // do not return system collections
          return;
        }
        if (c.get('type') === 'edge' && type === 'edges') {
          collections.push(c.id);
        } else if (c.get('type') === 'document' && type === 'vertices') {
          collections.push(c.id);
        }
      });

      const sorter = function (l, r) {
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
      return collections.sort(sorter);
    },

    getVerticesAutoCompletionList: function (editGraph) {
      return this.buildAutoCompletionLists('vertices', editGraph);
    },

    getEdgesAutoCompletionList: function (editGraph) {
      return this.buildAutoCompletionLists('edges', editGraph);
    },

    setGeneralGraphRows: function (forget, editGraph) {
      if (!editGraph) {
        this.setCacheModeState(forget);
      }

      this.hideSmartGraphRows();
      _.each(this.generalGraphRows, function (rowId) {
        $('#' + rowId).show();
      });
    },

    setSatelliteGraphRows: function (cache, editGraph) {
      $('#createGraph').addClass('active');
      if (!editGraph) {
        this.setCacheModeState(cache);
      }

      this.showGeneralGraphRows();
      this.hideSmartGraphRows();
      _.each(this.notNeededSatelliteGraphRows, function (rowId) {
        $('#' + rowId).hide();
      });
      $('#smartGraphInfo').show();
    },

    checkSmartGraphOneShardInfoHint: function () {
      var self = this;

      let showOneShardInfoHint = (result) => {
        if (result.sharding === 'single') {
          $('#' + self.potentiallyNeededSmartGraphRows[0]).show();
        }
      };

      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/database/current'),
        contentType: 'application/json',
        processData: false,
        async: true,
        success: function (data) {
          showOneShardInfoHint(data.result);
        },
        error: function (ignore) {
        }
      });
    },

    setSmartGraphRows: function (cache, id, editGraph) {
      $('#createGraph').addClass('active');
      if (!editGraph && cache) {
        this.setCacheModeState(cache);
      }

      this.hideGeneralGraphRows();
      this.checkSmartGraphOneShardInfoHint();
      _.each(this.neededSmartGraphRows, function (rowId) {
        $('#' + rowId).show();
      });

      // Extra rule to identify whether we need to display smartGraphAttribute field
      if (id === 'enterpriseGraph') {
        // In enterpriseGraph case is not allowed to be set
        _.each(this.nonNeededEnterpriseGraphRows, function (rowId) {
          $('#' + rowId).hide();
        });
      } else if (id === 'smartGraph') {
        // In smartGraph case it must be set
        _.each(this.nonNeededEnterpriseGraphRows, function (rowId) {
          $('#' + rowId).show();
        });
      }
    },

    hideSmartGraphRows: function () {
      _.each(this.neededSmartGraphRows, function (rowId) {
        $('#' + rowId).hide();
      });
      _.each(this.potentiallyNeededSmartGraphRows, function (rowId) {
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
        if (frontendConfig.isEnterprise) {
          this.createEditGraphModal();
        } else {
          this.createEditGraphModal();
          // hide tab entries
          // no EnterpriseGraphs in the community edition.
          $('#tab-enterpriseGraph').parent().remove();
          // no SmartGraphs in the community edition.
          $('#tab-smartGraph').parent().remove();
          // no SatelliteGraphs in the community edition.
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
      // Note: re-enable cached collections for GeneralGraph
      // General graph collections are allowed to use existing collections
      // SatelliteGraphs, SmartGraphs and Enterprise are not allowed to use them,
      // so we need to "forget" them here.
      let self = this;

      let i;
      for (i = 0; i < this.counter; i++) {
        $('#newEdgeDefinitions' + i).select2(self.buildSelect2Options('edge'));
        $('#newEdgeDefinitions' + i).select2('data', self.cachedNewEdgeDefinitions);
        $('#newEdgeDefinitions' + i).attr('disabled', self.cachedNewEdgeDefinitionsState);

        $('#fromCollections' + i).select2(self.buildSelect2Options('vertex'));
        $('#fromCollections' + i).select2('data', self.cachedFromCollections);
        $('#fromCollections' + i).attr('disabled', self.cachedFromCollectionsState);

        $('#toCollections' + i).select2(self.buildSelect2Options('vertex'));
        $('#toCollections' + i).select2('data', self.cachedToCollections);
        $('#toCollections' + i).attr('disabled', self.cachedToCollectionsState);
      }
      $('#newVertexCollections').select2(self.buildSelect2Options('vertex'));
      $('#newVertexCollections').select2('data', self.cachedNewVertexCollections);
      $('#newVertexCollections').attr('disabled', self.cachedNewVertexCollectionsState);
    },

    rememberCachedCollectionsState: function () {
      var self = this;
      var i;

      // TODO: Future: This whole section is not working anymore as it has been intended.
      // It worked once where we only had GeneralGraphs and SmartGraphs. Now also EnterpriseGraphs,
      // SatelliteGraphs and HybridGraphs are introduced. Therefore we have to manage more state of
      // all new graph types. The below code still needs to be available, as it re-initializes the
      // select2 components as well. But this area needs a whole cleanup. This can be done later.

      for (i = 0; i <= self.counter; i++) {
        $('#newEdgeDefinitions' + i).select2(self.buildSelect2Options('edge'));

        self.cachedNewEdgeDefinitions = $('#newEdgeDefinitions' + i).select2('data');
        self.cachedNewEdgeDefinitionsState = $('#newEdgeDefinitions' + i).attr('disabled');
        $('#newEdgeDefinitions' + i).select2('data', '');
        $('#newEdgeDefinitions' + i).attr('disabled', false);
        $('#newEdgeDefinitions' + i).change();

        $('#fromCollections' + i).select2(self.buildSelect2Options('vertex'));

        self.cachedFromCollections = $('#fromCollections' + i).select2('data');
        self.cachedFromCollectionsState = $('#fromCollections' + i).attr('disabled');
        $('#fromCollections' + i).select2('data', '');
        $('#fromCollections' + i).attr('disabled', false);
        $('#fromCollections' + i).change();

        $('#toCollections' + i).select2(self.buildSelect2Options('vertex'));
        self.cachedToCollections = $('#toCollections' + i).select2('data');
        self.cachedToCollectionsState = $('#toCollections' + i).attr('disabled');
        $('#toCollections' + i).select2('data', '');
        $('#toCollections' + i).attr('disabled', false);
        $('#toCollections' + i).change();
      }
      $('#newVertexCollections').select2(self.buildSelect2Options('vertex'));

      self.cachedNewVertexCollections = $('#newVertexCollections').select2('data');
      self.cachedNewVertexCollectionsState = $('#newVertexCollections').attr('disabled');
      $('#newVertexCollections').select2('data', '');
      $('#newVertexCollections').attr('disabled', false);
      $('#newVertexCollections').change();
    },

    setCacheModeState: function (forget) {
      if (!frontendConfig.isEnterprise) {
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
          self.events['click .graphViewer-icon-add-remove-button'] = self.addRemoveDefinition.bind(self);
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
      // This method triggers as soon as an edgeDefinition name is either being added or removed.
      e.stopPropagation();
      var id;

      const getAllCurrentEdgeDefinitionNames = () => {
        let edgeDefNames = [];
        for (let i = 0; i <= this.counter; i++) {
          let name = _.get($('#s2id_newEdgeDefinitions' + i).select2('data'), [0, 'text']);
          if (name) {
            // might be undefined as we do not have the state of all real "counter" values
            edgeDefNames.push(name);
          }
        }
        return edgeDefNames;
      };

      const toFindDuplicates = (arr) => {
        const itemCounts = arr.reduce((acc, item) => {
          acc[item] = (acc[item] || 0) + 1;
          return acc;
        }, {});

        return _.chain(arr).filter(item => itemCounts[item] > 1).uniq().value();
      };

      // This area checks whether new user defined EdgeDefinitions are being used twice - which is not valid.
      if (e.added) {
        let currentEdgeDefinitionName = e.added.id;
        const allAvailableEdgeDefinitionNames = getAllCurrentEdgeDefinitionNames();
        const duplicateEdgeDefinitions = toFindDuplicates(allAvailableEdgeDefinitionNames);

        if (duplicateEdgeDefinitions.indexOf(currentEdgeDefinitionName) !== -1) {
          id = e.currentTarget.id.split('row_newEdgeDefinitions')[1];
          $('input[id*="newEdgeDefinitions' + id + '"]').select2('val', null);
          arangoHelper.arangoError(
            "Graph Creation",
            `The EdgeDefinition name "${e.added.id}" is already being used. Please choose another name.`
          );

          // Please keep this line - we might need it later (during refactor)
          // Currently (the old way how this has been implemented) it is not usable
          // as the input keeps focus, we replace the placeholder - but as we still
          // do have the focus -> the user cannot see that is has changed meanwhile.
          // So for now, we remove that entry (as duplicate edge names are invalid)
          // and notify the user via "arangoHelper.arangoWarning".
          /*$('input[id*="newEdgeDefinitions' + id + '"]').attr(
            'placeholder', 'The collection ' + e.added.id + ' is already used.'
          );*/
          return;
        }
      }


      // This area checks whether a stored EdgeDefinition has been found and re-used here.
      // In case it is, we fill out from and to automatically and disable the inputs. As we
      // do not allow multiple EdgeDefinitions based on the same name.
      let map = this.calculateEdgeDefinitionMap();

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
        this.createEditGraphModal(graph, false, false, true);
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

      let edgeDefOptions = {};
      let satellites = $('#s2id_new-hybridSatelliteCollections').select2('data');
      if (satellites.length > 0) {
        edgeDefOptions.satellites = [];
        _.forEach(satellites, function(sat) {
          edgeDefOptions.satellites.push(sat.id);
        });
      }

      newEDs.forEach(
        function (eD) {
          graph.addEdgeDefinition(newEdgeDefinitions[eD], edgeDefOptions);
        }
      );

      editedEDs.forEach(
        function (eD) {
          graph.modifyEdgeDefinition(newEdgeDefinitions[eD], edgeDefOptions);
        }
      );

      deletedEDs.forEach(
        function (eD) {
          graph.deleteEdgeDefinition(eD);
        }
      );
      this.updateGraphManagementView();

      // TODO: Currently whenever a user modifies the current graph model, and any of the above calls:
      // "addEdgeDefinition", "modifyEdgeDefinition", "deleteEdgeDefinition" errors out -> we are notifying
      // the user (which is good), but we're also closing the modalView directly. So the user has no chance
      // to directly adjust his configuration to fix the misconfiguration. This needs to be improved.
      // We need to gather all API modification results from above, and are only allowed to close this modal
      // if all of them succeeded.
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
          return u.get('_key').toLowerCase().indexOf(searchString.toLowerCase()) !== -1;
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

      const smartGraphActive = $('#tab-smartGraph').parent().hasClass('active');
      const enterpriseGraphActive = $('#tab-enterpriseGraph').parent().hasClass('active');
      const graphTypeName = smartGraphActive ? this.smartGraphName : this.enterpriseGraphName;

      // SmartGraph && EnterpriseGraph area
      if (smartGraphActive || enterpriseGraphActive) {
        if ($('#new-numberOfShards').val() === '') {
          arangoHelper.arangoError(`${graphTypeName} creation`, 'numberOfShards not set!');
        }
        if (smartGraphActive) {
          if ($('#new-smartGraphAttribute').val() === '') {
            // Only needs to be set during SmartGraph creation
            arangoHelper.arangoError(`${graphTypeName} creation`, 'smartGraphAttribute not set!');
            return;
          }
        }

        newCollectionObject.isSmart = true;
        newCollectionObject.options = {
          numberOfShards: parseInt($('#new-numberOfShards').val()),
          replicationFactor: parseInt($('#new-replicationFactor').val()),
          minReplicationFactor: parseInt($('#new-writeConcern').val()),
          isDisjoint: $('#new-isDisjoint').is(':checked'),
          isSmart: true
        };
        if (smartGraphActive) {
          // Only needs to be set during SmartGraph creation
          newCollectionObject.options.smartGraphAttribute = $('#new-smartGraphAttribute').val();
        }

        let satellites = $('#s2id_hybridSatelliteCollections').select2('data');
        if (satellites.length > 0) {
          let satelliteOptions = [];
          _.forEach(satellites, function (sat) {
            satelliteOptions.push(sat.id);
          });
          newCollectionObject.options.satellites = satelliteOptions;
        }
      } else if ($('#tab-satelliteGraph').parent().hasClass('active')) {
        // SatelliteGraph enterprise area
        newCollectionObject.options = {
          replicationFactor: "satellite"
        };
      } else {
        if (frontendConfig.isCluster) {
          // General graph community cluster area
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
          // Hide potential error notifications, which might be obsolete now.
          arangoHelper.hideArangoNotifications();
          window.modalView.hide();
          arangoHelper.arangoNotification(
            'Graph',
            `Successfully created the graph: ${newCollectionObject.name}`
          );
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

    createEditGraphModal: function (graph, isSmart, isSatellite, isCommunity) {
      let isEnterpriseGraph = false;
      if (graph) {
        if (graph.get('isSmart') && !graph.get('smartGraphAttribute')) {
          // Found an EnterpriseGraph
          isEnterpriseGraph = true;
        }
      }
      const rowDescription = {
        graphName: {
          title: 'Name',
          description: 'String value. The name to identify the graph. Has to be unique and must follow the' +
            ' <b>Document Keys</b> naming conventions.',
          placeholder: 'Insert the name of the graph',
        },
        numberOfShards: {
          title: 'Shards',
          description: 'Numeric value. Must be at least 1. Number of shards the graph is using.',
          placeholder: 'Insert numeric value'
        },
        orphanCollection: {
          title: 'Orphan collections',
          description: 'Collections that are part of a graph but not used in an edge definition.',
          placeholder: 'Insert list of Orphan Collections'
        },
        replicationFactor: {
          title: 'Replication factor',
          description: 'Numeric value. Must be at least 1. Total number of copies of the data in the cluster.' +
            'If not given, the system default for new collections will be used.',
          placeholder: 'Insert numeric value'
        },
        writeConcern: {
          title: 'Write concern',
          description: 'Numeric value. Must be at least 1. Must be smaller or equal compared to the replication ' +
            'factor. Total number of copies of the data in the cluster that are required for each write operation. ' +
            'If we get below this value, the collection will be read-only until enough copies are created. ' +
            'If not given, the system default for new collections will be used.',
          placeholder: 'Insert numeric value'
        },
        smartGraphAttribute: {
          title: 'SmartGraph Attribute',
          description: 'String value. The attribute name that is used to smartly shard the vertices of a graph.' +
            ' Every vertex in this Graph has to have this attribute.',
          placeholder: 'Insert the smartGraphAttribute'
        },
        satelliteCollections: {
          title: 'Satellite collections',
          descriptionNew: 'Insert vertex collections here which are being used in new edge definitions (fromCollections, toCollections).' +
            ' Those defined collections will be created as SatelliteCollections, and therefore will be replicated to all DB-Servers.',
          descriptionEdit: 'Insert vertex collections here which are being used in your edge definitions (fromCollections, toCollections).' +
            ' Those defined collections will be created as SatelliteCollections, and therefore will be replicated to all DB-Servers.',
          placeholder: 'Insert list of <from>/<to> Vertex Collections'
        },
        edgeDefinitions: {
          title: 'Edge definition',
          description: 'An edge definition defines a relation of the graph.',
          placeholder: 'Insert a single Edge Collection'
        },
        toVertexCollection: {
          title: 'toCollections',
          description: 'The collections that contain the end vertices of the relation.',
          placeholder: 'Insert list of <to> Vertex Collections'
        },
        fromVertexCollections: {
          title: 'fromCollections',
          description: 'The collections that contain the start vertices of the relation.',
          placeholder: 'Insert list of <from> Vertex Collections'
        }
      };

      var buttons = [];
      var tableContent = [];
      var self = this;
      var name = '';
      var edgeDefinitions = [{collection: '', from: '', to: ''}];
      var hybridSatelliteCollections = '';
      var orphanCollections = '';
      var title;

      this.counter = 0;

      // edit graph section
      if (graph) {
        let isEnterpriseGraph = false;

        if (isSmart) {
          if (!graph.get("smartGraphAttribute")) {
            isEnterpriseGraph = true;
          }
          if (!isEnterpriseGraph) {
            title = 'Edit SmartGraph';
            this.currentGraphEditType = 'smart';
          } else {
            // If smartGraphAttribute not available, it must be an EnterpriseGraph
            title = 'Edit EnterpriseGraph';
            this.currentGraphEditType = 'enterprise';
          }
        } else if (isSatellite) {
          title = 'Edit SatelliteGraph';
          this.currentGraphEditType = 'satellite';
        } else {
          title = 'Edit Graph';
          this.currentGraphEditType = 'community';
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
            rowDescription.graphName.title,
            name,
            rowDescription.graphName.description
          )
        );

        if (isSmart && !isEnterpriseGraph) {
          tableContent.push(
            window.modalView.createReadOnlyEntry(
              'smartGraphAttribute',
              rowDescription.smartGraphAttribute.title,
              graph.get('smartGraphAttribute'),
              rowDescription.smartGraphAttribute.description
            )
          );
        }

        if (graph.get('numberOfShards')) {
          tableContent.push(
            window.modalView.createReadOnlyEntry(
              'numberOfShards',
              rowDescription.numberOfShards.title,
              graph.get('numberOfShards'),
              rowDescription.numberOfShards.description
            )
          );
        }

        if (graph.get('replicationFactor')) {
          tableContent.push(
            window.modalView.createReadOnlyEntry(
              'replicationFactor',
              rowDescription.replicationFactor.title,
              graph.get('replicationFactor'),
              rowDescription.replicationFactor.description
            )
          );
        }

        if (graph.get('minReplicationFactor')) {
          tableContent.push(
            window.modalView.createReadOnlyEntry(
              'writeConcern',
              rowDescription.writeConcern.title,
              graph.get('minReplicationFactor'),
              rowDescription.writeConcern.description
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

          tableContent.push(
            window.modalView.createSelect2Entry(
              'new-hybridSatelliteCollections',
              'New Satellite collections',
              hybridSatelliteCollections,
              rowDescription.satelliteCollections.descriptionNew,
              'New Satellite Collections',
              false,
              false,
              false,
              null,
              self.getEdgesAutoCompletionList(),
              null, // style
              null, // cssClass
              self.isEnterpriseOnlyGraphOrIsDefaultIsEnterprise(this.currentGraphEditType)
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
        this.currentGraphEditType = undefined;

        // create graph section
        title = 'Create Graph';

        tableContent.push(
          window.modalView.createTextEntry(
            'createNewGraphName',
            rowDescription.graphName.title,
            '',
            rowDescription.graphName.description,
            rowDescription.graphName.placeholder,
            true
          )
        );

        buttons.push(
          window.modalView.createSuccessButton('Create', this.createNewGraph.bind(this))
        );
      }

      if (frontendConfig.isEnterprise === true && !graph) {
        // Enterprise Graphs
        tableContent.push(
          window.modalView.createTextEntry(
            'new-numberOfShards',
            rowDescription.numberOfShards.title + '*',
            '',
            rowDescription.numberOfShards.description,
            rowDescription.numberOfShards.placeholder,
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
            rowDescription.replicationFactor.title,
            '',
            rowDescription.replicationFactor.description,
            rowDescription.replicationFactor.placeholder,
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
            rowDescription.writeConcern.title,
            '',
            rowDescription.writeConcern.description,
            rowDescription.writeConcern.placeholder,
            false,
            [
              {
                rule: Joi.string().allow('').optional().regex(/^[1-9]*$/),
                msg: rowDescription.writeConcern.description
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
            rowDescription.smartGraphAttribute.title + '*',
            '',
            rowDescription.smartGraphAttribute.description + ' Cannot be modified later.',
            rowDescription.smartGraphAttribute.placeholder,
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
        // General Graphs Community
        tableContent.push(
          window.modalView.createTextEntry(
            'general-numberOfShards',
            rowDescription.numberOfShards.title,
            '',
            rowDescription.numberOfShards.description,
            rowDescription.numberOfShards.placeholder,
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
            rowDescription.replicationFactor.title,
            '',
            rowDescription.replicationFactor.description,
            rowDescription.replicationFactor.placeholder,
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
            rowDescription.writeConcern.title,
            '',
            rowDescription.writeConcern.description,
            rowDescription.writeConcern.placeholder,
            false,
            [
              {
                rule: Joi.string().allow('').optional().regex(/^[1-9]*$/),
                msg: 'Must be a number. Must be at least 1 and has to be smaller or equal compared to the replicationFactor.'
              }
            ]
          )
        );
        tableContent.push(
          window.modalView.createSelect2Entry(
            'hybridSatelliteCollections',
            rowDescription.satelliteCollections.title,
            hybridSatelliteCollections,
            rowDescription.satelliteCollections.descriptionEdit,
            rowDescription.satelliteCollections.placeholder,
            false,
            false,
            false,
            null,
            self.getEdgesAutoCompletionList(),
            null, // style
            null, // cssClass
            self.isEnterpriseOnlyGraphOrIsDefaultIsEnterprise()
          )
        );
      }

      const createEdgeDefinitionEntry = (counter, edgeDefinition, graph) => {
        let id = null;
        let isMandatory = true;
        let addDelete = false;
        let addAdd = true;

        if (counter !== 0) {
          // means we're not creating the initial entry
          id =  'newEdgeDefinitions' + self.counter;
          isMandatory = false;
          addDelete = true;
          addAdd = false;
        }

        tableContent.push(
          window.modalView.createSpacerEntry(
            null, 'Relation'
          )
        );
        tableContent.push(
          window.modalView.createSelect2Entry(
            'newEdgeDefinitions' + self.counter,
            rowDescription.edgeDefinitions.title,
            edgeDefinition.collection,
            rowDescription.edgeDefinitions.description,
            rowDescription.edgeDefinitions.placeholder,
            isMandatory,
            addDelete,
            addAdd,
            1,
            self.getEdgesAutoCompletionList(graph ? self.currentGraphEditType : undefined),
            undefined, /*style*/
            'first', /*cssClass*/
            self.isEnterpriseOnlyGraphOrIsDefaultIsEnterprise(graph ? self.currentGraphEditType : undefined)
          )
        );
      };

      edgeDefinitions.forEach(
        function (edgeDefinition) {
          // EdgeDefinition MAIN entry
          createEdgeDefinitionEntry(self.counter, edgeDefinition, graph);
          // EdgeDefinition FROM entry
          tableContent.push(
            window.modalView.createSelect2Entry(
              rowDescription.fromVertexCollections.title + self.counter,
              rowDescription.fromVertexCollections.title,
              edgeDefinition.from,
              rowDescription.fromVertexCollections.description,
              rowDescription.fromVertexCollections.placeholder,
              true,
              false,
              false,
              null,
              self.getVerticesAutoCompletionList(graph ? self.currentGraphEditType : undefined),
              undefined, /*style*/
              'middle', /*cssClass*/
              self.isEnterpriseOnlyGraphOrIsDefaultIsEnterprise(graph ? self.currentGraphEditType : undefined)
            )
          );
          // EdgeDefinition TO entry
          tableContent.push(
            window.modalView.createSelect2Entry(
              rowDescription.toVertexCollection.title + self.counter,
              rowDescription.toVertexCollection.title,
              edgeDefinition.to,
              rowDescription.toVertexCollection.description,
              rowDescription.toVertexCollection.placeholder,
              true,
              false,
              false,
              null,
              self.getVerticesAutoCompletionList(graph ? self.currentGraphEditType : undefined),
              undefined, /*style*/
              'last', /*cssClass*/
              self.isEnterpriseOnlyGraphOrIsDefaultIsEnterprise(graph ? self.currentGraphEditType : undefined)
            )
          );
          self.counter++;
        }
      );

      tableContent.push(
        window.modalView.createSelect2Entry(
          'newVertexCollections',
          rowDescription.orphanCollection.title,
          orphanCollections,
          rowDescription.orphanCollection.description,
          rowDescription.orphanCollection.placeholder,
          false,
          false,
          false,
          null,
          self.getVerticesAutoCompletionList(graph ? self.currentGraphEditType : undefined),
          undefined,
          undefined,
          self.isEnterpriseOnlyGraphOrIsDefaultIsEnterprise(graph ? self.currentGraphEditType : undefined)
        )
      );

      window.modalView.show(
        'modalGraphTable.ejs', title, buttons, tableContent, undefined, undefined, this.events
      );

      if ($('#tab-createGraph').parent().hasClass('active')) {
        // hide them by default, as we're showing general graph as default
        // satellite does not need to appear here as it has no additional input fields
        self.hideSmartGraphRows();
      } else if ($('#tab-enterpriseGraph').parent().hasClass('active')) {
        // In enterpriseGraph case is not allowed to be set
        // Special stunt here, as we need to generate that row to be generally available,
        // but we need to hide it as "enterpriseGraphs" is the new default (if we do have
        // an enterprise instance running).
        self.setSmartGraphRows(null, 'enterpriseGraph');
      } else if ($('#tab-smartGraph').parent().hasClass('active')) {
        self.setSmartGraphRows(null, 'smartGraph');
      }

      if (graph) {
        // Means we're in the editGraph state.
        if (isCommunity) {
          this.setGeneralGraphRows(false, true);
        } else if (isSatellite) {
          this.setSatelliteGraphRows(false, true);
        } else if (isEnterpriseGraph) {
          this.setSmartGraphRows(false, 'enterpriseGraph', true);
        } else if (isSmart) {
          this.setSmartGraphRows(false, 'smartGraph', true);
        }

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

    buildSelect2Options: function (type) {
      let collections = [];
      if (type === 'edge') {
        collections = this.getEdgesAutoCompletionList(
          this.currentGraphEditType ? this.currentGraphEditType : undefined
        );
      } else if (type === 'vertex') {
        collections = this.getVerticesAutoCompletionList(
          this.currentGraphEditType ? this.currentGraphEditType : undefined
        );
      }

      let options = {
        tags: collections,
        showSearchBox: false,
        minimumResultsForSearch: -1,
        width: '336px',
      };

      if (type === 'edge') {
        options.maximumSelectionSize =  1;
      }

      if (this.isEnterpriseOnlyGraphOrIsDefaultIsEnterprise(
        this.currentGraphEditType ? this.currentGraphEditType : undefined)
      ) {
        options.language = {};
        options.language.noMatches = function () {
          return "Please enter a new and valid collection name.";
        };
      } else {
        options.language = {};
        options.language.noMatches = function () {
          return "No collections found.";
        };
      }

      return options;
    },

    addRemoveDefinition: function (e) {
      e.stopPropagation();
      let id = $(e.currentTarget).attr('id');
      let number;

      if (id.indexOf('addAfter_newEdgeDefinitions') !== -1) {
        this.counter++;
        $('#row_newVertexCollections').before(
          this.edgeDefintionTemplate.render({
            number: this.counter
          })
        );
        $('#newEdgeDefinitions' + this.counter).select2(this.buildSelect2Options('edge'));
        $('#fromCollections' + this.counter).select2(this.buildSelect2Options('vertex'));
        $('#toCollections' + this.counter).select2(this.buildSelect2Options('vertex'));
        window.modalView.undelegateEvents();
        window.modalView.delegateEvents(this.events);

        arangoHelper.fixTooltips('.icon_arangodb, .arangoicon', 'right');

        let i;
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
        $('#row_newEdgeDefinitionsSpacer' + number).remove();
        $('#row_newEdgeDefinitions' + number).remove();
        $('#row_fromCollections' + number).remove();
        $('#row_toCollections' + number).remove();
        $('#spacer' + number).remove();
      }
    },

    calculateEdgeDefinitionMap: function () {
      // This method calculates and returns all the edge definitions of all graphs.
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
