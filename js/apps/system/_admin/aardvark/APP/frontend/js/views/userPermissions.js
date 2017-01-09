/* jshint browser: true */
/* jshint unused: false */
/* global _, frontendConfig, arangoHelper, Backbone, window, templateEngine, $ */

(function () {
  'use strict';

  window.UserPermissionView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate('userPermissionView.ejs'),

    initialize: function (options) {
      this.username = options.username;
    },

    events: {
      'click #userPermissionView [type="checkbox"]': 'setPermission'
    },

    render: function () {
      var self = this;

      this.collection.fetch({
        success: function () {
          self.continueRender();
        }
      });
    },

    setPermission: function (e) {
      var checked = $(e.currentTarget).is(':checked');
      var db = $(e.currentTarget).attr('name');

      if (checked) {
        this.grantPermission(this.currentUser.get('user'), db);
      } else {
        if (db === '_system') {
          // special case, ask if user really want to revoke persmission here
          var buttons = []; var tableContent = [];

          tableContent.push(
            window.modalView.createReadOnlyEntry(
              'db-system-revoke-button',
              'Caution',
              'You are removing your permissions to _system database. Really continue?',
              undefined,
              undefined,
              false
            )
          );
          buttons.push(
            window.modalView.createSuccessButton('Revoke', this.revokePermission.bind(this, this.currentUser.get('user'), db))
          );
          buttons.push(
            window.modalView.createCloseButton('Cancel', this.rollbackInputButton.bind(this, db))
          );
          window.modalView.show('modalTable.ejs', 'Revoke _system Database Permission', buttons, tableContent);
        } else {
          this.revokePermission(this.currentUser.get('user'), db);
        }
      }
    },

    rollbackInputButton: function (name) {
      $('input[name="' + name + '"').prop('checked', 'true');
    },

    grantPermission: function (user, db) {
      $.ajax({
        type: 'PUT',
        url: arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(user) + '/database/' + encodeURIComponent(db)),
        contentType: 'application/json',
        data: JSON.stringify({
          grant: 'rw'
        })
      });
    },

    revokePermission: function (user, db) {
      $.ajax({
        type: 'PUT',
        url: arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(user) + '/database/' + encodeURIComponent(db)),
        contentType: 'application/json'
      });
      window.modalView.hide();
    },

    continueRender: function () {
      var self = this;

      this.currentUser = this.collection.findWhere({
        user: this.username
      });

      this.breadcrumb();

      arangoHelper.buildUserSubNav(this.currentUser.get('user'), 'Permissions');

      var url = arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(self.currentUser.get('user')) + '/database');
      if (frontendConfig.db === '_system') {
        url = arangoHelper.databaseUrl('/_api/user/root/database');
      }

      // FETCH COMPLETE DB LIST
      $.ajax({
        type: 'GET',
        url: url,
        contentType: 'application/json',
        success: function (data) {
          var allDBs = data.result;

          // NOW FETCH USER PERMISSIONS
          $.ajax({
            type: 'GET',
            url: arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(self.currentUser.get('user')) + '/database'),
            // url: arangoHelper.databaseUrl("/_api/database/user/" + encodeURIComponent(this.currentUser.get("user"))),
            contentType: 'application/json',
            success: function (data) {
              var permissions = data.result;
              if (allDBs._system) {
                var arr = [];
                _.each(allDBs, function (db, name) {
                  arr.push(name);
                });
                allDBs = arr;
              }
              self.finishRender(allDBs, permissions);
            }
          });
        }
      });
    },

    finishRender: function (allDBs, permissions) {
      _.each(permissions, function (value, key) {
        if (value !== 'rw') {
          delete permissions[key];
        }
      });

      $(this.el).html(this.template.render({
        allDBs: allDBs,
        permissions: permissions
      }));
    },

    breadcrumb: function () {
      $('#subNavigationBar .breadcrumb').html(
        'User: ' + this.currentUser.get('user')
      );
    }

  });
}());
