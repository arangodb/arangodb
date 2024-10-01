/* global frontendConfig */

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
        'Collection: ' + _.escape(this.collectionName.length > 64 ? this.collectionName.substr(0, 64) + "..." : this.collectionName)
      );
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
              window.App.navigate('#collections', { trigger: true });
              window.arangoHelper.arangoNotification(
                "Collection successfully dropped"
              );
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
              newname = String($('#change-collection-name').val()).normalize();
            }

            var self = this;

            var callbackChange = function (error, data) {
              if (error) {
                self.render();
                arangoHelper.arangoError('Collection error: ' + data.responseJSON.errorMessage);
              } else {
                arangoHelper.arangoNotification('Collection: ' + 'Successfully changed.');
                window.App.navigate('#cSettings/' + encodeURIComponent(newname), {trigger: true});
              }
            };

            var callbackRename = function (error) {
              var abort = false;
              if (error) {
                arangoHelper.arangoError('Collection error: ' + error.responseText);
              } else {
                var wfs = $('#change-collection-sync').val();
                var cacheEnabled = $('#change-collection-cache').val();
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
                  this.model.changeCollection(wfs, replicationFactor, writeConcern, cacheEnabled, callbackChange);
                }
              }
            }.bind(this);

            if (frontendConfig.isCluster === false) {
              this.model.renameCollection(newname, callbackRename);
            } else {
              callbackRename();
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

          var buttons = [];
          var tableContent = [];
          var collectionNameValidations = 
            window.arangoValidationHelper.getCollectionNameValidations();
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
                  collectionNameValidations
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
                  collectionNameValidations
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

            buttons.push(
              window.modalView.createSuccessButton(
                'Save',
                this.saveModifiedCollection.bind(this)
              )
            );
            var templates = ['modalTable.ejs'];
            var tabBar = ['General', 'Indexes'];
            var isCurrentView =
              window.location.hash.indexOf(
                "cSettings/" + encodeURIComponent(this.collectionName)
              ) > -1;
            if (!isCurrentView) {
              return;
            }
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

          var callback2 = function (error, data) {
            if (error) {
              arangoHelper.arangoError('Collection', 'Could not fetch properties');
            } else {
              var wfs = data.waitForSync;
              if (data.replicationFactor && frontendConfig.isCluster) {
                var reason = null;
                var hint = '';
                if (data.replicationFactor === 'satellite') {
                  reason = 'This collection is a SatelliteCollection.';
                } else if (data.distributeShardsLike !== undefined) {
                  reason = 'This collection uses \'distributeShardsLike\'.';
                  hint = ' The value can be changed in collection \'' + data.distributeShardsLike + '\' instead.';
                }

                if (reason !== null) {
                  tableContent.push(
                    window.modalView.createReadOnlyEntry(
                      'change-replication-factor',
                      'Replication factor',
                      data.replicationFactor,
                      reason + ' The replication factor is not changeable for this collection.' + hint,
                      '',
                      true
                    )
                  );
                  tableContent.push(
                    window.modalView.createReadOnlyEntry(
                      'change-write-concern',
                      'Write concern',
                      JSON.stringify(data.writeConcern),
                      reason + ' The write concern is not changeable for this collection.' + hint,
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
                          rule: Joi.string().allow('').optional().regex(/^[1-9][0-9]*$/),
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
              
              tableContent.push(
                window.modalView.createSelectEntry(
                  'change-collection-cache',
                  'Enable documents cache',
                  data.cacheEnabled,
                  'Enable in-memory cache for documents and primary index',
                  [{value: false, label: 'No'}, {value: true, label: 'Yes'}])
              );
            }
            after();

            // check permissions and adjust views
            arangoHelper.checkCollectionPermissions(self.collectionName, self.changeViewToReadOnly.bind(this));
          };

          this.model.getProperties(callback2);
        }
      }.bind(this);
      window.isCoordinator(callback);
    }

  });
}());
