/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, _, Backbone, window, templateEngine, $ */

(function () {
  'use strict';

  window.UserPermissionView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate('userPermissionView.ejs'),

    initialize: function (options) {
      this.username = options.username;
    },

    remove: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    events: {
      'click #userPermissionView .dbCheckbox': 'setDBPermission',
      'click #userPermissionView .collCheckbox': 'setCollPermission',
      'click .db-row': 'toggleAccordion'
    },

    render: function (open, error) {
      var self = this;

      this.collection.fetch({
        fetchAllUsers: true,
        success: function () {
          self.continueRender(open, error);
        }
      });
    },

    toggleAccordion: function (e) {
      // if checkbox was hit
      if ($(e.target).attr('type')) {
        return;
      }
      if ($(e.target).parent().hasClass('noAction')) {
        return;
      }
      if ($(e.target).hasClass('inner') || $(e.target).is('span') || $(e.target).hasClass('fa-info-circle')) {
        return;
      }
      var visible = $(e.currentTarget).find('.collection-row').is(':visible');

      var db = $(e.currentTarget).attr('id').split('-')[0];
      $('.collection-row').hide();

      // unhighlight db labels
      $('.db-label').css('font-weight', 200);
      $('.db-label').css('color', '#8a969f');

      // if collections are available
      if ($(e.currentTarget).find('.collection-row').children().length > 4) {
        // if menu was already visible -> then hide
        $('.db-row .fa-caret-down').hide();
        $('.db-row .fa-caret-right').show();
        if (visible) {
          $(e.currentTarget).find('.collection-row').hide();
        } else {
          // else show menu
          $(e.currentTarget).find('.collection-row').fadeIn('fast');
          // highlight db label
          $(e.currentTarget).find('.db-label').css('font-weight', 600);
          $(e.currentTarget).find('.db-label').css('color', 'rgba(64, 74, 83, 1)');
          // caret animation
          $(e.currentTarget).find('.fa-caret-down').show();
          $(e.currentTarget).find('.fa-caret-right').hide();
        }
      } else {
        // caret animation
        $('.db-row .fa-caret-down').hide();
        $('.db-row .fa-caret-right').show();
        arangoHelper.arangoNotification('Permissions', 'No collections in "' + db + '" available.');
      }
    },

    setCollPermission: function (e) {
      var db = $(e.currentTarget).attr('db');
      var collection = $(e.currentTarget).attr('collection');
      var value;

      if ($(e.currentTarget).hasClass('readOnly')) {
        value = 'ro';
      } else if ($(e.currentTarget).hasClass('readWrite')) {
        value = 'rw';
      } else if ($(e.currentTarget).hasClass('noAccess')) {
        value = 'none';
      } else {
        value = 'undefined';
      }
      this.sendCollPermission(this.currentUser.get('user'), db, collection, value);
    },

    setDBPermission: function (e) {
      var db = $(e.currentTarget).attr('name');

      var value;
      if ($(e.currentTarget).hasClass('readOnly')) {
        value = 'ro';
      } else if ($(e.currentTarget).hasClass('readWrite')) {
        value = 'rw';
      } else if ($(e.currentTarget).hasClass('noAccess')) {
        value = 'none';
      } else {
        value = 'undefined';
      }

      if (db === '_system') {
        // special case, ask if user really want to revoke persmission here
        var buttons = []; var tableContent = [];

        tableContent.push(
          window.modalView.createReadOnlyEntry(
            'db-system-revoke-button',
            'Caution',
            'You are changing the _system database permission. Really continue?',
            undefined,
            undefined,
            false
          )
        );
        buttons.push(
          window.modalView.createSuccessButton('Ok', this.sendDBPermission.bind(this, this.currentUser.get('user'), db, value))
        );
        buttons.push(
          window.modalView.createCloseButton('Cancel', this.rollbackInputButton.bind(this, db))
        );
        window.modalView.show('modalTable.ejs', 'Change _system Database Permission', buttons, tableContent);
      } else {
        this.sendDBPermission(this.currentUser.get('user'), db, value);
      }
    },

    rollbackInputButton: function (name, error) {
      // this method will refetch server permission state and re-render everything again from scratch
      var open;
      _.each($('.collection-row'), function (elem, key) {
        if ($(elem).is(':visible')) {
          open = $(elem).parent().attr('id');
        }
      });

      if (open) {
        this.render(open, error);
      } else {
        this.render();
      }
      window.modalView.hide();
    },

    sendCollPermission: function (user, db, collection, value) {
      var self = this;

      if (value === 'undefined') {
        this.revokeCollPermission(user, db, collection);
      } else {
        $.ajax({
          type: 'PUT',
          url: arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(user) + '/database/' + encodeURIComponent(db) + '/' + encodeURIComponent(collection)),
          contentType: 'application/json',
          data: JSON.stringify({
            grant: value
          }),
          success: function () {
            self.styleDefaultRadios(null, true);
          },
          error: function (e) {
            self.rollbackInputButton(null, e);
          }
        });
      }
    },

    revokeCollPermission: function (user, db, collection) {
      var self = this;

      $.ajax({
        type: 'DELETE',
        url: arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(user) + '/database/' + encodeURIComponent(db) + '/' + encodeURIComponent(collection)),
        contentType: 'application/json',
        success: function (e) {
          self.styleDefaultRadios(null, true);
        },
        error: function (e) {
          self.rollbackInputButton(null, e);
        }
      });
    },

    sendDBPermission: function (user, db, value) {
      var self = this;

      if (value === 'undefined') {
        this.revokeDBPermission(user, db);
      } else {
        $.ajax({
          type: 'PUT',
          url: arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(user) + '/database/' + encodeURIComponent(db)),
          contentType: 'application/json',
          data: JSON.stringify({
            grant: value
          }),
          success: function () {
            self.styleDefaultRadios(null, true);
          },
          error: function (e) {
            self.rollbackInputButton(null, e);
          }
        });
      }
    },

    revokeDBPermission: function (user, db) {
      var self = this;

      $.ajax({
        type: 'DELETE',
        url: arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(user) + '/database/' + encodeURIComponent(db)),
        contentType: 'application/json',
        success: function (e) {
          self.styleDefaultRadios(null, true);
        },
        error: function (e) {
          self.rollbackInputButton(null, e);
        }
      });
    },

    continueRender: function (open, error) {
      var self = this;

      this.currentUser = this.collection.findWhere({
        user: this.username
      });

      var url = arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(self.currentUser.get('user')) + '/database?full=true');

      // FETCH COMPLETE DB LIST
      $.ajax({
        type: 'GET',
        url: url,
        contentType: 'application/json',
        success: function (data) {
          // fetching available dbs and permissions
          self.finishRender(data.result, open, error);
        },
        error: function (data) {
          arangoHelper.arangoError('User', 'Could not fetch user permissions');
        }
      });
    },

    finishRender: function (permissions, open, error) {
      // sort permission databases
      var sortedArr = _.pairs(permissions);
      sortedArr.sort();
      sortedArr = _.object(sortedArr);

      $(this.el).html(this.template.render({
        permissions: sortedArr
      }));
      // * wildcard at the end
      $('.noAction').first().appendTo('.pure-table-body');
      $('.pure-table-body').height(window.innerHeight - 200);
      if (open) {
        $('#' + open).click();
      }
      if (error && error.responseJSON && error.responseJSON.errorMessage) {
        arangoHelper.arangoError('User', error.responseJSON.errorMessage);
      }
      // style default radio boxes
      this.styleDefaultRadios(permissions);
      // tooltips
      arangoHelper.createTooltips();
      // check if current user is root
      this.checkRoot();
      this.breadcrumb();
    },

    checkRoot: function () {
      // disable * in _system db for root user, because it is not allowed to lower permissions there
      if (this.currentUser.get('user') === 'root') {
        $('#_system-db #___-collection input').attr('disabled', 'true');
      }
    },

    styleDefaultRadios: function (permissions, refresh) {
      var serverLevelDefaultPermission;
        try {
          serverLevelDefaultPermission = permissions['*'].permission;
        } catch (ignore) {
          // just ignore, not part of the response
        }
      var self = this;

      var getMaxPermissionAccess = function (databaseLevel, collectionLevel, serverLevel) {
        // will return max permission level
        if (databaseLevel === 'undefined' && collectionLevel === 'undefined' && serverLevel !== 'undefined') {
          return serverLevel;
        } else if (collectionLevel === 'undefined' || collectionLevel === 'none') {
          // means - use default is selected here
          if (serverLevel === 'rw' || databaseLevel === 'rw') {
            return 'rw';
          } else if (serverLevel === 'ro' || databaseLevel === 'ro') {
            return 'ro';
          } else if (serverLevel === 'none' || databaseLevel === 'none') {
            return 'none';
          } else {
            return 'undefined';
          }
        } else if (serverLevel === 'rw' || databaseLevel === 'rw' || collectionLevel === 'rw') {
          return 'rw';
        } else if (serverLevel === 'ro' || databaseLevel === 'ro' || collectionLevel === 'ro') {
          return 'ro';
        } else if (databaseLevel === 'none' || collectionLevel === 'none') {
          if (serverLevel !== 'undefined') {
            return serverLevel;
          }
          return 'none';
        } else {
          return 'undefined';
        }
      };

      var someFunction = function (permissions) {
        $('.db-row input').css('box-shadow', 'none');
        // var cssShadow = '#2ecc71 0px 1px 4px 4px';
        var cssShadow = 'rgba(0, 0, 0, 0.3) 0px 1px 4px 4px';
        _.each(permissions, function (perms, database) {
          if (perms.collections) {
            // Default Database Permission (wildcard)
            var databaseLevelDefaultPermission = perms.permission;
            // Default Collection Permission (wildcard)
            var collectionLevelDefaultPermission = perms.collections['*'];

            if (databaseLevelDefaultPermission === 'undefined') {
              if (serverLevelDefaultPermission) {
                if (serverLevelDefaultPermission === 'rw') {
                  $('#' + database + '-db .mid > .readWrite').first().css('box-shadow', cssShadow);
                } else if (serverLevelDefaultPermission === 'ro') {
                  $('#' + database + '-db .mid > .readOnly').first().css('box-shadow', cssShadow);
                } else if (serverLevelDefaultPermission === 'none') {
                  $('#' + database + '-db .mid > .noAccess').first().css('box-shadow', cssShadow);
                }
              }
            }

            _.each(perms.collections, function (access, collection) {
              if (collection.charAt(0) !== '_' && collection.charAt(0) !== '*') {
                if (access === 'undefined') {
                  // This means, we have no specific value set, we need now to compare if collection level wildcard is set and
                  // if database level wildcard is set and choose the max of it to display.
                  var calculatedCollectionPermission = getMaxPermissionAccess(databaseLevelDefaultPermission, collectionLevelDefaultPermission, serverLevelDefaultPermission);
                  if (calculatedCollectionPermission === 'rw') {
                    $('#' + database + '-db #' + collection + '-collection .readWrite').css('box-shadow', cssShadow);
                  } else if (calculatedCollectionPermission === 'ro') {
                    $('#' + database + '-db #' + collection + '-collection .readOnly').css('box-shadow', cssShadow);
                  } else if (calculatedCollectionPermission === 'none') {
                    $('#' + database + '-db #' + collection + '-collection .noAccess').css('box-shadow', cssShadow);
                  }
                }
              }
            });
          }
        });
      };

      if (refresh) {
        var url = arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(self.currentUser.get('user')) + '/database?full=true');
        // FETCH COMPLETE DB LIST
        $.ajax({
          type: 'GET',
          url: url,
          contentType: 'application/json',
          success: function (data) {
            someFunction(data.result);
          },
          error: function (data) {
            arangoHelper.arangoError('User', 'Could not fetch user permissions');
          }
        });
      } else {
        someFunction(permissions);
      }
      window.modalView.hide();
    },

    breadcrumb: function () {
      var self = this;

      if (window.App.naviView) {
        $('#subNavigationBar .breadcrumb').html(
          'User: ' + arangoHelper.escapeHtml(this.currentUser.get('user'))
        );
        arangoHelper.buildUserSubNav(self.currentUser.get('user'), 'Permissions');
      } else {
        window.setTimeout(function () {
          self.breadcrumb();
        }, 100);
      }
    }

  });
}());
