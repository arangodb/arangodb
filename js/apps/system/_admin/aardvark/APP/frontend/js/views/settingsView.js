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
        'Collection: ' + this.collectionName
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
            error: function () {
              arangoHelper.arangoError('Could not delete collection.');
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
              var journalSize;
              if (frontendConfig.engine !== 'rocksdb') {
                try {
                  journalSize = JSON.parse($('#change-collection-size').val() * 1024 * 1024);
                } catch (e) {
                  arangoHelper.arangoError('Please enter a valid journal size number.');
                  return 0;
                }
              }

              var indexBuckets;
              if (frontendConfig.engine !== 'rocksdb') {
                try {
                  indexBuckets = JSON.parse($('#change-index-buckets').val());
                  if (indexBuckets < 1 || parseInt(indexBuckets, 10) !== Math.pow(2, Math.log2(indexBuckets))) {
                    throw new Error('invalid indexBuckets value');
                  }
                } catch (e) {
                  arangoHelper.arangoError('Please enter a valid number of index buckets.');
                  return 0;
                }
              }
              var callbackChange = function (error) {
                if (error) {
                  arangoHelper.arangoError('Collection error: ' + error.responseText);
                } else {
                  arangoHelper.arangoNotification('Collection: ' + 'Successfully changed.');
                  window.App.navigate('#cSettings/' + newname, {trigger: true});
                }
              };

              var callbackRename = function (error) {
                if (error) {
                  arangoHelper.arangoError('Collection error: ' + error.responseText);
                } else {
                  var wfs = $('#change-collection-sync').val();
                  this.model.changeCollection(wfs, journalSize, indexBuckets, callbackChange);
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
            buttons.push(
              window.modalView.createNotificationButton(
                'Load Indexes in Memory',
                this.warmupCollection.bind(this)
              )
            );
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
                if (data.journalSize) {
                  var journalSize = data.journalSize / (1024 * 1024);
                  var indexBuckets = data.indexBuckets;
                  var wfs = data.waitForSync;

                  tableContent.push(
                    window.modalView.createTextEntry(
                      'change-collection-size',
                      'Journal size',
                      journalSize,
                      'The maximal size of a journal or datafile (in MB). Must be at least 1.',
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
                }

                if (indexBuckets) {
                  tableContent.push(
                    window.modalView.createTextEntry(
                      'change-index-buckets',
                      'Index buckets',
                      indexBuckets,
                      'The number of index buckets for this collection. Must be at least 1 and a power of 2.',
                      '',
                      true,
                      [
                        {
                          rule: Joi.string().allow('').optional().regex(/^[1-9][0-9]*$/),
                          msg: 'Must be a number greater than 1 and a power of 2.'
                        }
                      ]
                    )
                  );
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
