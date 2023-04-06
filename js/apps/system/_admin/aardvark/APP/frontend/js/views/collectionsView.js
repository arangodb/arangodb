/* jshint browser: true */
/* jshint unused: false */
/* global frontendConfig, _, Backbone, templateEngine, window, arangoHelper, Joi, $ */

(function () {
  'use strict';
  window.CollectionsView = Backbone.View.extend({
    el: '#content',
    el2: '#collectionsThumbnailsIn',
    readOnly: false,
    defaultReplicationFactor: 0,
    minReplicationFactor: 0,
    maxReplicationFactor: 0,
    maxNumberOfShards: 0,

    searchTimeout: null,
    refreshRate: 10000,

    template: templateEngine.createTemplate('collectionsView.ejs'),

    remove: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    refetchCollections: function () {
      var self = this;
      this.collection.fetch({
        cache: false,
        success: function () {
          self.checkLockedCollections();
        }
      });
    },

    checkLockedCollections: function () {
      var callback = function (error, lockedCollections) {
        var self = this;
        if (!error) {
          this.collection.each(function (model) {
            model.set('locked', false);
          });

          _.each(lockedCollections, function (locked) {
            var model = self.collection.findWhere({
              id: locked.collection
            });
            model.set('locked', true);
            model.set('lockType', locked.type);
            model.set('desc', locked.desc);
          });

          this.collection.each(function (model) {
            $('#collection_' + model.get('name')).removeClass('locked');
            if ($('#collection_' + model.get('name') + ' .corneredBadge').hasClass('inProgress')) {
              $('#collection_' + model.get('name') + ' .corneredBadge').removeClass('inProgress');
            }
          });
        }
      }.bind(this);

      if (!frontendConfig.ldapEnabled) {
        window.arangoHelper.syncAndReturnUnfinishedAardvarkJobs('index', callback);
      }
    },

    initialize: function () {
      var self = this;

      this.defaultReplicationFactor = frontendConfig.defaultReplicationFactor;
      this.minReplicationFactor = frontendConfig.minReplicationFactor;
      this.maxReplicationFactor = frontendConfig.maxReplicationFactor;
      this.maxNumberOfShards = frontendConfig.maxNumberOfShards;

      window.setInterval(function () {
        if (window.location.hash === '#collections' && window.VISIBLE) {
          self.refetchCollections();
        }
      }, self.refreshRate);
    },

    render: function () {
      this.checkLockedCollections();
      var dropdownVisible = false;

      if ($('#collectionsDropdown').is(':visible')) {
        dropdownVisible = true;
      }

      $(this.el).html(this.template.render({}));
      this.setFilterValues();

      if (dropdownVisible === true) {
        $('#collectionsDropdown2').show();
      }

      var searchOptions = this.collection.searchOptions;

      this.collection.getFiltered(searchOptions).forEach(function (arangoCollection) {
        $('#collectionsThumbnailsIn', this.el).append(new window.CollectionListItemView({
          model: arangoCollection,
          collectionsView: this
        }).render().el);
      }, this);

      // if type in collectionsDropdown2 is changed,
      // the page will be rerendered, so check the toggel button
      if ($('#collectionsDropdown2').css('display') === 'none') {
        $('#collectionsToggle').removeClass('activated');
      } else {
        $('#collectionsToggle').addClass('activated');
      }

      var length;
      arangoHelper.setCheckboxStatus('#collectionsDropdown');

      try {
        length = searchOptions.searchPhrase.length;
      } catch (ignore) {}
      $('#searchInput').val(searchOptions.searchPhrase);
      $('#searchInput').focus();
      $('#searchInput')[0].setSelectionRange(length, length);

      arangoHelper.fixTooltips('.icon_arangodb, .arangoicon', 'left');
      arangoHelper.checkDatabasePermissions(this.setReadOnly.bind(this));

      return this;
    },

    setReadOnly: function () {
      this.readOnly = true;
      $('#createCollection').parent().parent().addClass('disabled');
    },

    events: {
      'click #createCollection': 'createCollection',
      'keydown #searchInput': 'restrictToSearchPhraseKey',
      'change #searchInput': 'restrictToSearchPhrase',
      'click #searchSubmit': 'restrictToSearchPhrase',
      'click .checkSystemCollections': 'checkSystem',
      'click #checkDocument': 'checkDocument',
      'click #checkEdge': 'checkEdge',
      'click #sortName': 'sortName',
      'click #sortType': 'sortType',
      'click #sortOrder': 'sortOrder',
      'click #collectionsToggle': 'toggleView',
      'click .css-label': 'checkBoxes'
    },

    updateCollectionsView: function () {
      var self = this;
      this.collection.fetch({
        cache: false,
        success: function () {
          self.render();
        }
      });
    },

    toggleView: function () {
      $('#collectionsToggle').toggleClass('activated');
      $('#collectionsDropdown2').slideToggle(200);
    },

    checkBoxes: function (e) {
      // chrome bugfix
      var clicked = e.currentTarget.id;
      $('#' + clicked).click();
    },

    checkSystem: function () {
      var searchOptions = this.collection.searchOptions;
      var oldValue = searchOptions.includeSystem;

      searchOptions.includeSystem = ($('.checkSystemCollections').is(':checked') === true);

      if (oldValue !== searchOptions.includeSystem) {
        this.render();
      }
    },
    checkEdge: function () {
      var searchOptions = this.collection.searchOptions;
      var oldValue = searchOptions.includeEdge;

      searchOptions.includeEdge = ($('#checkEdge').is(':checked') === true);

      if (oldValue !== searchOptions.includeEdge) {
        this.render();
      }
    },
    checkDocument: function () {
      var searchOptions = this.collection.searchOptions;
      var oldValue = searchOptions.includeDocument;

      searchOptions.includeDocument = ($('#checkDocument').is(':checked') === true);

      if (oldValue !== searchOptions.includeDocument) {
        this.render();
      }
    },
    sortName: function () {
      var searchOptions = this.collection.searchOptions;
      var oldValue = searchOptions.sortBy;

      searchOptions.sortBy = (($('#sortName').is(':checked') === true) ? 'name' : 'type');
      if (oldValue !== searchOptions.sortBy) {
        this.render();
      }
    },
    sortType: function () {
      var searchOptions = this.collection.searchOptions;
      var oldValue = searchOptions.sortBy;

      searchOptions.sortBy = (($('#sortType').is(':checked') === true) ? 'type' : 'name');
      if (oldValue !== searchOptions.sortBy) {
        this.render();
      }
    },
    sortOrder: function () {
      var searchOptions = this.collection.searchOptions;
      var oldValue = searchOptions.sortOrder;

      searchOptions.sortOrder = (($('#sortOrder').is(':checked') === true) ? -1 : 1);
      if (oldValue !== searchOptions.sortOrder) {
        this.render();
      }
    },

    setFilterValues: function () {
      var searchOptions = this.collection.searchOptions;
      $('.checkSystemCollections').attr('checked', searchOptions.includeSystem);
      $('#checkEdge').attr('checked', searchOptions.includeEdge);
      $('#checkDocument').attr('checked', searchOptions.includeDocument);
      $('#sortName').attr('checked', searchOptions.sortBy !== 'type');
      $('#sortType').attr('checked', searchOptions.sortBy === 'type');
      $('#sortOrder').attr('checked', searchOptions.sortOrder !== 1);
    },

    search: function () {
      var searchOptions = this.collection.searchOptions;
      var searchPhrase = $('#searchInput').val();
      if (searchPhrase === searchOptions.searchPhrase) {
        return;
      }
      searchOptions.searchPhrase = searchPhrase;

      this.render();
    },

    resetSearch: function () {
      if (this.searchTimeout) {
        clearTimeout(this.searchTimeout);
        this.searchTimeout = null;
      }

      var searchOptions = this.collection.searchOptions;
      searchOptions.searchPhrase = null;
    },

    restrictToSearchPhraseKey: function (event) {
      if (
        event && event.originalEvent && (
          (
            event.originalEvent.key &&
            (
              event.originalEvent.key === 'Control' ||
              event.originalEvent.key === 'Alt' ||
              event.originalEvent.key === 'Shift'
            )
          ) ||
          event.originalEvent.ctrlKey ||
          event.originalEvent.altKey
        )
      ) {
        return;
      }
      // key pressed in search box
      var self = this;

      // force a new a search
      this.resetSearch();

      self.searchTimeout = setTimeout(function () {
        self.search();
      }, 200);
    },

    restrictToSearchPhrase: function () {
      // force a new a search
      this.resetSearch();

      // search executed
      this.search();
    },

    createCollection: function (e) {
      if (!this.readOnly) {
        e.preventDefault();
        var self = this;

        $.ajax({
          type: 'GET',
          cache: false,
          url: arangoHelper.databaseUrl('/_api/engine'),
          contentType: 'application/json',
          processData: false,
          success: function (data) {
            self.engine = data;
            self.createNewCollectionModal(data);
          },
          error: function () {
            arangoHelper.arangoError('Engine', 'Could not fetch ArangoDB Engine details.');
          }
        });
      }
    },

    submitCreateCollection: function (isOneShardDB) {
      var self = this;
      var callbackCoord = function (error, isCoordinator) {
        if (error) {
          arangoHelper.arangoError('DB', 'Could not check coordinator state');
        } else {
          var collName = $('#new-collection-name').val();
          var collSize = $('#new-collection-size').val();
          var replicationFactor = Number($('#new-replication-factor').val());
          var writeConcern = Number($('#new-write-concern').val());
          var collType = $('#new-collection-type').val();
          var collSync = $('#new-collection-sync').val();
          var shards = 1;
          var shardKeys = [];
          var smartJoinAttribute = '';
          var distributeShardsLike = '';

          if (replicationFactor === '') {
            if (self.defaultReplicationFactor) {
              replicationFactor = self.defaultReplicationFactor;
            } else {
              replicationFactor = 1;
            }
          }
          if (writeConcern === '') {
            writeConcern = 1;
          }
          if ($('#is-satellite-collection').val() === 'true') {
            replicationFactor = 'satellite';
          }

          if (isCoordinator) {
            if (frontendConfig.isEnterprise && $('#smart-join-attribute').val() !== '') {
              smartJoinAttribute = $('#smart-join-attribute').val().trim();
            }
            if (frontendConfig.isEnterprise) {
              try {
                // field may be entirely hidden
                distributeShardsLike = $('#distribute-shards-like').val().trim();
              } catch (err) {
              }
            }

            // number of shards field may be read-only, in this case we just assume 1
            try {
              shards = $('#new-collection-shards').val();
            } catch (err) {
              shards = 1;
            }

            if (shards === '') {
              shards = 1;
            }
            shards = parseInt(shards, 10);
            if (shards < 1) {
              arangoHelper.arangoError(
                'Number of shards has to be an integer value greater or equal 1'
              );
              return 0;
            }
            shardKeys = _.pluck($('#new-collection-shardKeys').select2('data'), 'text');
            if (shardKeys.length === 0) {
              shardKeys.push('_key');
            } else {
              _.each(shardKeys, function (element, index) { shardKeys[index] = arangoHelper.escapeHtml(element); });
            }
          }
          if (collName.substr(0, 1) === '_') {
            arangoHelper.arangoError('No "_" allowed as first character!');
            return 0;
          }
          var isSystem = false;
          var wfs = (collSync === 'true');
          if (collSize > 0) {
            try {
              collSize = JSON.parse(collSize) * 1024 * 1024;
            } catch (e) {
              arangoHelper.arangoError('Please enter a valid number');
              return 0;
            }
          }
          if (collName === '') {
            arangoHelper.arangoError('No collection name entered!');
            return 0;
          }
          // no new system collections via webinterface
          // var isSystem = (collName.substr(0, 1) === '_')
          var callback = function (error, data) {
            if (error) {
              try {
                data = JSON.parse(data.responseText);
                arangoHelper.arangoError('Error', data.errorMessage);
              } catch (ignore) {}
            } else {
              if (window.location.hash === '#collections') {
                this.updateCollectionsView();
              }
              arangoHelper.arangoNotification('Collection', 'Collection "' + data.name + '" successfully created.');
            }
          }.bind(this);

          var abort = false;
          try {
            if (Number.parseInt(writeConcern) > Number.parseInt(replicationFactor)) {
              // validation here, as our Joi integration misses some core features
              arangoHelper.arangoError("New Collection", "Write concern is not allowed to be greater than replication factor");
              abort = true;
            }
          } catch (ignore) {
          }

          var tmpObj = {
            collName: collName,
            wfs: wfs,
            isSystem: isSystem,
            collType: collType
          };

          if (!isOneShardDB) {
            if (smartJoinAttribute !== '') {
              tmpObj.smartJoinAttribute = smartJoinAttribute;
            }

            // If we are in a oneShardDB we are not allowed to set those values
            // They are always inferred
            if (window.App.isCluster) {
              tmpObj.shardKeys = shardKeys;
              if (distributeShardsLike === '') {
                // if we are not using distribute shards like
                // then we want to make use of the given shard information
                tmpObj.shards = shards;
                tmpObj.replicationFactor = replicationFactor === "satellite" ? replicationFactor : Number(replicationFactor);
                tmpObj.writeConcern = Number(writeConcern);
              } else {
                // If we use distribute shards like on purpose do not add other
                // sharding information. All of it will be deferred.
                tmpObj.distributeShardsLike = distributeShardsLike;
              }
            }
          }

          if (!abort) {
            this.collection.newCollection(tmpObj, callback);
            window.modalView.hide();
            arangoHelper.arangoNotification('Collection', 'Collection "' + collName + '" will be created.');
          }
        }
      }.bind(this);

      window.isCoordinator(callbackCoord);
    },

    createNewCollectionModal: function () {
        var self = this;
        $.ajax({
          type: 'GET',
          cache: false,
          url: arangoHelper.databaseUrl('/_api/database/current'), //get default properties of current db
          contentType: 'application/json',
          processData: false,
          success: function (data) {
            self.createNewCollectionModalReal(data.result);
          },
          error: function () {
            arangoHelper.arangoError('Engine', 'Could not fetch default collection properties.');
          }
        });

    },

    createNewCollectionModalReal: function (properties) {
      var self = this;
      var callbackCoord2 = function (error, isCoordinator) {
        if (error) {
          arangoHelper.arangoError('DB', 'Could not check coordinator state');
        } else {
          var buttons = [];
          var tableContent = [];
          var advanced = {};
          var advancedTableContent = [];

          tableContent.push(
            window.modalView.createTextEntry(
              'new-collection-name',
              'Name',
              '',
              false,
              '',
              true,
              [
                {
                  rule: Joi.string().required(),
                  msg: 'No collection name given.'
                }
              ]
            )
          );

          tableContent.push(
            window.modalView.createSelectEntry(
              'new-collection-type',
              'Type',
              '',
              'Use the Document type to store json documents with unique _key attributes. Can be used as nodes in a graph. <br> Use the Edge type to store documents with special edge attributes (_to, _from), which represent relations. Can be used as edges in a graph.',
              [{value: 2, label: 'Document'}, {value: 3, label: 'Edge'}]
            )
          );

          if (isCoordinator) {
            var allowEdit = properties.sharding !== 'single' && !frontendConfig.forceOneShard;
            if (allowEdit) {
              tableContent.push(
                window.modalView.createTextEntry(
                  'new-collection-shards',
                  'Number of shards',
                  this.maxNumberOfShards === 1 ? String(this.maxNumberOfShards) : 0,
                  'The number of shards to create. The maximum value is ' + this.maxNumberOfShards + '. You cannot change this afterwards.',
                  '',
                  true
                )
              );
            } else {
              tableContent.push(
                window.modalView.createReadOnlyEntry(
                  'new-collection-shards-readonly',
                  'Number of shards',
                  this.maxNumberOfShards === 1 ? String(this.maxNumberOfShards) : 1,
                  ''
                )
              );
            }
            tableContent.push(
              window.modalView.createSelect2Entry(
                'new-collection-shardKeys',
                'Shard keys',
                '',
                'The keys used to distribute documents on shards. ' +
                'Type the key and press return to add it.',
                '_key',
                false
              )
            );

            if (window.App.isCluster) {
              var minReplicationFactor = (this.minReplicationFactor ? this.minReplicationFactor : 1);
              var maxReplicationFactor = (this.maxReplicationFactor ? this.maxReplicationFactor : 10);

              // clamp replicationFactor between min & max allowed values
              var replicationFactor = '';
              if (properties.replicationFactor) {
                replicationFactor = parseInt(properties.replicationFactor);
                if (replicationFactor < minReplicationFactor) {
                  replicationFactor = minReplicationFactor;
                }
                if (replicationFactor > maxReplicationFactor) {
                  replicationFactor = maxReplicationFactor;
                }
              }

              tableContent.push(
                window.modalView.createTextEntry(
                  'new-replication-factor',
                  'Replication factor',
                  String(replicationFactor),
                  'Numeric value. Must be between ' + minReplicationFactor + ' and ' +
                  maxReplicationFactor + '. Total number of copies of the data in the cluster',
                  '',
                  false,
                  [
                    {
                      rule: Joi.string().allow('').optional().regex(/^[1-9][0-9]*$/),
                      msg: 'Must be a number between ' + minReplicationFactor +
                           ' and ' + maxReplicationFactor + '.'
                    }
                  ]
                )
              );
            }
          }

          buttons.push(
            window.modalView.createSuccessButton(
              'Save',
              this.submitCreateCollection.bind(this, properties.sharding === 'single' || frontendConfig.forceOneShard)
            )
          );
          if (window.App.isCluster) {
            if (frontendConfig.isEnterprise) {
              if (properties.sharding !== 'single' && !frontendConfig.forceOneShard) {
                advancedTableContent.push(
                  window.modalView.createTextEntry(
                    'distribute-shards-like',
                    'Distribute shards like',
                    '',
                    'Name of another collection that should be used as a prototype for sharding this collection.',
                    '',
                    false,
                    [
                    ]
                  )
                );
              }
              advancedTableContent.push(
                window.modalView.createSelectEntry(
                  'is-satellite-collection',
                  'SatelliteCollection',
                  '',
                  'Use this option for smaller data sets that will be synchronously replicated to each DB-Server, facilitating local join operations. Selecting it will disable the replication factor above.',
                  [{value: false, label: 'No'}, {value: true, label: 'Yes'}]
                )
              );
              advancedTableContent.push(
                window.modalView.createTextEntry(
                  'smart-join-attribute',
                  'SmartJoin attribute',
                  '',
                  'String attribute name. Can be left empty if SmartJoins are not used.',
                  '',
                  false,
                  [
                  ]
                )
              );
            }

            advancedTableContent.push(
              window.modalView.createTextEntry(
                'new-write-concern',
                'Write concern',
                properties.writeConcern ? properties.writeConcern : '',
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
          advancedTableContent.push(
            window.modalView.createSelectEntry(
              'new-collection-sync',
              'Wait for sync',
              '',
              'Synchronize to disk before returning from a create or update of a document.',
              [{value: false, label: 'No'}, {value: true, label: 'Yes'}]
            )
          );
          advanced.header = 'Advanced';
          advanced.content = advancedTableContent;
          window.modalView.show(
            'modalTable.ejs',
            'New Collection',
            buttons,
            tableContent,
            advanced
          );

          // select2 workaround
          $('#s2id_new-collection-shardKeys .select2-search-field input').on('focusout', function (e) {
            if ($('.select2-drop').is(':visible')) {
              if (!$('#select2-search-field input').is(':focus')) {
                window.setTimeout(function () {
                  $(e.currentTarget).parent().parent().parent().select2('close');
                }, 200);
              }
            }
          });

          if (window.App.isCluster && frontendConfig.isEnterprise) {
            var handleSatelliteIds = [
              '#new-replication-factor',
              '#new-write-concern',
              '#new-collection-shards',
              '#smart-join-attribute'
            ];

            $('#is-satellite-collection').on('change', function (element) {
              if ($('#is-satellite-collection').val() === 'true') {
                _.each(handleSatelliteIds, function(id) {
                  $(id).prop('disabled', true);
                });
                $('#s2id_new-collection-shardKeys').select2('disable');
              } else {
                _.each(handleSatelliteIds, function(id) {
                  $(id).prop('disabled', false);
                });
                $('#s2id_new-collection-shardKeys').select2('enable');
              }
              _.each(handleSatelliteIds, function(id) {
                $(id).val('').focus().focusout();
              });
            });

            $('#distribute-shards-like').on('input', function (element) {
              $('#new-collection-shards').prop('disabled', element.target.value !== '');
              $('#new-replication-factor').prop('disabled', element.target.value !== '');
            });
          }
        }
      }.bind(this);

      window.isCoordinator(callbackCoord2);
    }
  });
}());
