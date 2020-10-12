/* jshint browser: true */
/* jshint unused: false */
/* global frontendConfig, arangoHelper, Joi, Backbone, window, $ */

(function () {
  'use strict';

  window.SettingsView = Backbone.View.extend({
    el: '#content',
    readOnly: false,

    initialize: function (options) {
      this.collectionName = options.collectionName;
      this.model = this.collection;
    },

    events: {
    },

    render: function () {
      this.breadcrumb();
      arangoHelper.buildCollectionSubNav(this.collectionName, 'Settings');

      this.renderSettings();
    },

    breadcrumb: function () {
      $('#subNavigationBar .breadcrumb').html(
        'Collection: ' + (this.collectionName.length > 64 ? this.collectionName.substr(0, 64) + "..." : this.collectionName)
      );
    },

    unloadCollection: function () {
      if (!this.readOnly) {
        var unloadCollectionCallback = function (error) {
          if (error) {
            arangoHelper.arangoError('Collection error', this.model.get('name') + ' could not be unloaded.');
          } else if (error === undefined) {
            this.model.set('status', 'unloading');
            this.render();
          } else {
            if (window.location.hash === '#collections') {
              this.model.set('status', 'unloaded');
              this.render();
            } else {
              arangoHelper.arangoNotification('Collection ' + this.model.get('name') + ' unloaded.');
            }
          }
        }.bind(this);

        this.model.unloadCollection(unloadCollectionCallback);
        window.modalView.hide();
      }
    },

    loadCollection: function () {
      if (!this.readOnly) {
        var loadCollectionCallback = function (error) {
          if (error) {
            arangoHelper.arangoError('Collection error', this.model.get('name') + ' could not be loaded.');
          } else if (error === undefined) {
            this.model.set('status', 'loading');
            this.render();
          } else {
            if (window.location.hash === '#collections') {
              this.model.set('status', 'loaded');
              this.render();
            } else {
              arangoHelper.arangoNotification('Collection ' + this.model.get('name') + ' loaded.');
            }
          }
        }.bind(this);

        this.model.loadCollection(loadCollectionCallback);
        window.modalView.hide();
      }
    },

    truncateCollection: function () {
      if (!this.readOnly) {
        this.model.truncateCollection();
        $('.modal-delete-confirmation').hide();
        window.modalView.hide();
      }
    },

    warmupCollection: function () {
      if (!this.readOnly) {
        this.model.warmupCollection();
        $('.modal-delete-confirmation').hide();
        window.modalView.hide();
      }
    },

    deleteCollection: function () {
      if (!this.readOnly) {
        this.model.destroy(
          {
            error: function (_, data) {
              arangoHelper.arangoError('Could not drop collection: ' + data.responseJSON.errorMessage);
            },
            success: function () {
              window.App.navigate('#collections', {trigger: true});
            }
          }
        );
      }
    },

    saveModifiedCollection: function () {
      if (!this.readOnly) {
        var callback = function (error, isCoordinator) {
          if (error) {
            arangoHelper.arangoError('Error', 'Could not get coordinator info');
          } else {
            var newname;
            if (isCoordinator) {
              newname = this.model.get('name');
            } else {
              newname = $('#change-collection-name').val();
            }
            var status = this.model.get('status');

            if (status === 'loaded') {
              var self = this;

              var callbackChange = function (error, data) {
                if (error) {
                  self.render();
                  arangoHelper.arangoError('Collection error: ' + data.responseJSON.errorMessage);
                } else {
                  arangoHelper.arangoNotification('Collection: ' + 'Successfully changed.');
                  window.App.navigate('#cSettings/' + newname, {trigger: true});
                }
              };

              var callbackRename = function (error) {
                var abort = false;
                if (error) {
                  arangoHelper.arangoError('Collection error: ' + error.responseText);
                } else {
                  var wfs = $('#change-collection-sync').val();
                  var replicationFactor;
                  var writeConcern;

                  if (frontendConfig.isCluster) {
                    replicationFactor = $('#change-replication-factor').val();
                    writeConcern = $('#change-write-concern').val();
                    try {
                      if (Number.parseInt(writeConcern) > Number.parseInt(replicationFactor)) {
                        // validation here, as our Joi integration misses some core features
                        arangoHelper.arangoError("Change Collection", "Write concern is not allowed to be greater than replication factor");
                        abort = true;
                      }
                    } catch (ignore) {
                    }
                  }
                  if (!abort) {
                    this.model.changeCollection(wfs, replicationFactor, writeConcern, callbackChange);
                  }
                }
              }.bind(this);

              if (frontendConfig.isCluster === false) {
                this.model.renameCollection(newname, callbackRename);
              } else {
                callbackRename();
              }
            } else if (status === 'unloaded') {
              if (this.model.get('name') !== newname) {
                var callbackRename2 = function (error, data) {
                  if (error) {
                    arangoHelper.arangoError('Collection' + data.responseText);
                  } else {
                    arangoHelper.arangoNotification('Collection' + 'Successfully changed.');
                    window.App.navigate('#cSettings/' + newname, {trigger: true});
                  }
                };

                if (frontendConfig.isCluster === false) {
                  this.model.renameCollection(newname, callbackRename2);
                } else {
                  callbackRename2();
                }
              } else {
                window.modalView.hide();
              }
            }
          }
        }.bind(this);

        window.isCoordinator(callback);
      }
    },

    changeViewToReadOnly: function () {
      window.App.settingsView.readOnly = true;
      $('.breadcrumb').html($('.breadcrumb').html() + ' (read-only)');
      // this method disables all write-based functions
      $('.modal-body input').prop('disabled', 'true');
      $('.modal-body select').prop('disabled', 'true');
      $('.modal-footer button').addClass('disabled');
      $('.modal-footer button').unbind('click');
    },

    renderSettings: function () {
      var self = this;

      var callback = function (error, isCoordinator) {
        if (error) {
          arangoHelper.arangoError('Error', 'Could not get coordinator info');
        } else {
          var collectionIsLoaded = false;

          if (this.model.get('status') === 'loaded') {
            collectionIsLoaded = true;
          }

          var buttons = [];
          var tableContent = [];

          if (!isCoordinator) {
            if (this.model.get('name').substr(0, 1) === '_') {
              tableContent.push(
                window.modalView.createReadOnlyEntry(
                  'change-collection-name',
                  'Name',
                  this.model.get('name'),
                  false,
                  '',
                  true,
                  [
                    {
                      rule: Joi.string().regex(/^[a-zA-Z]/),
                      msg: 'Collection name must always start with a letter.'
                    },
                    {
                      rule: Joi.string().regex(/^[a-zA-Z0-9\-_]*$/),
                      msg: 'Only Symbols "_" and "-" are allowed.'
                    },
                    {
                      rule: Joi.string().required(),
                      msg: 'No collection name given.'
                    }
                  ]
                )
              );
            } else {
              tableContent.push(
                window.modalView.createTextEntry(
                  'change-collection-name',
                  'Name',
                  this.model.get('name'),
                  false,
                  '',
                  true,
                  [
                    {
                      rule: Joi.string().regex(/^[a-zA-Z]/),
                      msg: 'Collection name must always start with a letter.'
                    },
                    {
                      rule: Joi.string().regex(/^[a-zA-Z0-9\-_]*$/),
                      msg: 'Only Symbols "_" and "-" are allowed.'
                    },
                    {
                      rule: Joi.string().required(),
                      msg: 'No collection name given.'
                    }
                  ]
                )
              );
            }
          }

          var after = function () {
            tableContent.push(
              window.modalView.createReadOnlyEntry(
                'change-collection-id', 'ID', this.model.get('id'), ''
              )
            );
            tableContent.push(
              window.modalView.createReadOnlyEntry(
                'change-collection-type', 'Type', this.model.get('type'), ''
              )
            );
            tableContent.push(
              window.modalView.createReadOnlyEntry(
                'change-collection-status', 'Status', this.model.get('status'), ''
              )
            );
            buttons.push(
              window.modalView.createDeleteButton(
                'Delete',
                this.deleteCollection.bind(this)
              )
            );
            buttons.push(
              window.modalView.createDeleteButton(
                'Truncate',
                this.truncateCollection.bind(this)
              )
            );
            if (frontendConfig.engine === 'rocksdb') {
              buttons.push(
                window.modalView.createNotificationButton(
                  'Load Indexes into Memory',
                  this.warmupCollection.bind(this)
                )
              );
            }
            if (collectionIsLoaded) {
              buttons.push(
                window.modalView.createNotificationButton(
                  'Unload',
                  this.unloadCollection.bind(this)
                )
              );
            } else {
              buttons.push(
                window.modalView.createNotificationButton(
                  'Load',
                  this.loadCollection.bind(this)
                )
              );
            }

            buttons.push(
              window.modalView.createSuccessButton(
                'Save',
                this.saveModifiedCollection.bind(this)
              )
            );

            var tabBar = ['General', 'Indexes'];
            var templates = ['modalTable.ejs', 'indicesView.ejs'];

            window.modalView.show(
              templates,
              'Modify Collection',
              buttons,
              tableContent, null, null,
              this.events, null,
              tabBar, 'content'
            );
            $($('#infoTab').children()[1]).remove();
          }.bind(this);

          if (collectionIsLoaded) {
            var callback2 = function (error, data) {
              if (error) {
                arangoHelper.arangoError('Collection', 'Could not fetch properties');
              } else {
                var wfs = data.waitForSync;
                if (data.replicationFactor && frontendConfig.isCluster) {
                  if (data.replicationFactor === 'satellite') {
                    tableContent.push(
                      window.modalView.createReadOnlyEntry(
                        'change-replication-factor',
                        'Replication factor',
                        data.replicationFactor,
                        'This collection is a SatelliteCollection. The replicationFactor is not changeable.',
                        '',
                        true
                      )
                    );
                    tableContent.push(
                      window.modalView.createReadOnlyEntry(
                        'change-write-concern',
                        'Write concern',
                        JSON.stringify(data.writeConcern),
                        'This collection is a SatelliteCollection. The write concern is not changeable.',
                        '',
                        true
                      )
                    );
                  } else {
                    tableContent.push(
                      window.modalView.createTextEntry(
                        'change-replication-factor',
                        'Replication factor',
                        data.replicationFactor,
                        'The replicationFactor parameter is the total number of copies being kept, that is, it is one plus the number of followers. Must be a number.',
                        '',
                        true,
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
                        'change-write-concern',
                        'Write concern',
                        data.writeConcern,
                        'Numeric value. Must be at least 1. Must be smaller or equal compared to the replication factor. Total number of copies of the data in the cluster that are required for each write operation. If we get below this value the collection will be read-only until enough copies are created.',
                        '',
                        true,
                        [
                          {
                            rule: Joi.string().allow('').optional().regex(/^[1-9]*$/),
                            msg: 'Must be a number. Must be at least 1 and has to be smaller or equal compared to the replicationFactor.'
                          }
                        ]
                      )
                    );
                  }
                }

                // prevent "unexpected sync method error"
                tableContent.push(
                  window.modalView.createSelectEntry(
                    'change-collection-sync',
                    'Wait for sync',
                    wfs,
                    'Synchronize to disk before returning from a create or update of a document.',
                    [{value: false, label: 'No'}, {value: true, label: 'Yes'}])
                );
              }
              after();

              // check permissions and adjust views
              arangoHelper.checkCollectionPermissions(self.collectionName, self.changeViewToReadOnly.bind(this));
            };

            this.model.getProperties(callback2);
          } else {
            after();
            // check permissions and adjust views
            arangoHelper.checkCollectionPermissions(self.collectionName, self.changeViewToReadOnly.bind(this));
          }
        }
      }.bind(this);
      window.isCoordinator(callback);
    }

  });
}());
