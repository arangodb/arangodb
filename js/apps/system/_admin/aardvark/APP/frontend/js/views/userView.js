/* jshint browser: true */
/* jshint unused: false */
/* global CryptoJS, _, arangoHelper, Backbone, window $ */

(function () {
  'use strict';

  window.UserView = Backbone.View.extend({
    el: '#content',

    initialize: function (options) {
      this.username = options.username;
    },

    render: function () {
      var self = this;

      this.collection.fetch({
        success: function () {
          self.continueRender();
        }
      });
    },

    editCurrentUser: function () {
      this.createEditCurrentUserModal(
        this.currentUser.get('user'),
        this.currentUser.get('extra').name,
        this.currentUser.get('extra').img
      );
    },

    continueRender: function () {
      this.breadcrumb();

      this.currentUser = this.collection.findWhere({
        user: this.username
      });

      arangoHelper.buildUserSubNav(this.currentUser.get('user'), 'General');

      if (this.currentUser.get('loggedIn')) {
        this.editCurrentUser();
      } else {
        this.createEditUserModal(
          this.currentUser.get('user'),
          this.currentUser.get('extra').name,
          this.currentUser.get('active')
        );
      }
    },

    createEditUserPasswordModal: function () {
      var buttons = [];
      var tableContent = [];

      tableContent.push(
        window.modalView.createPasswordEntry(
          'newCurrentPassword',
          'New Password',
          '',
          false,
          'new password',
          false
        )
      );
      tableContent.push(
        window.modalView.createPasswordEntry(
          'confirmCurrentPassword',
          'Confirm New Password',
          '',
          false,
          'confirm new password',
          false)
      );

      buttons.push(
        window.modalView.createSuccessButton(
          'Save',
          this.submitEditUserPassword.bind(this)
        )
      );

      window.modalView.show('modalTable.ejs', 'Edit User Password', buttons, tableContent);
    },

    createEditCurrentUserModal: function (username, name, img) {
      var buttons = [];
      var tableContent = [];

      tableContent.push(
        window.modalView.createReadOnlyEntry('id_username', 'Username', username)
      );
      tableContent.push(
        window.modalView.createTextEntry('editCurrentName', 'Name', name, false, 'Name', false)
      );
      tableContent.push(
        window.modalView.createTextEntry(
          'editCurrentUserProfileImg',
          'Gravatar account (Mail)',
          img,
          'Mailaddress or its md5 representation of your gravatar account.' +
          'The address will be converted into a md5 string. ' +
          'Only the md5 string will be stored, not the mailaddress.',
          'myAccount(at)gravatar.com'
        )
      );

      buttons.push(
        window.modalView.createNotificationButton(
          'Change Password',
          this.editUserPassword.bind(this)
        )
      );
      buttons.push(
        window.modalView.createSuccessButton(
          'Save',
          this.submitEditCurrentUserProfile.bind(this)
        )
      );

      // window.modalView.show("modalTable.ejs", "Edit User Profile", buttons, tableContent)

      window.modalView.show(
        'modalTable.ejs',
        'Edit User Profile',
        buttons,
        tableContent, null, null,
        this.events, null,
        null, 'content'
      );
    },

    parseImgString: function (img) {
      // if already md5
      if (img.indexOf('@') === -1) {
        return img;
      }
      // else generate md5
      return CryptoJS.MD5(img).toString();
    },

    createEditUserModal: function (username, name, active) {
      var buttons, tableContent;
      tableContent = [
        {
          type: window.modalView.tables.READONLY,
          label: 'Username',
          value: _.escape(username)
        },
        {
          type: window.modalView.tables.TEXT,
          label: 'Name',
          value: name,
          id: 'editName',
          placeholder: 'Name'
        },
        {
          type: window.modalView.tables.CHECKBOX,
          label: 'Active',
          value: 'active',
          checked: active,
          id: 'editStatus'
        }
      ];
      buttons = [
        {
          title: 'Delete',
          type: window.modalView.buttons.DELETE,
          callback: this.submitDeleteUser.bind(this, username)
        },
        {
          title: 'Change Password',
          type: window.modalView.buttons.NOTIFICATION,
          callback: this.createEditUserPasswordModal.bind(this, username)
        },
        {
          title: 'Save',
          type: window.modalView.buttons.SUCCESS,
          callback: this.submitEditUser.bind(this, username)
        }
      ];

      window.modalView.show(
        'modalTable.ejs',
        'Edit User',
        buttons,
        tableContent, null, null,
        this.events, null,
        null, 'content'
      );
    },

    validateStatus: function (status) {
      if (status === '') {
        return false;
      }
      return true;
    },

    submitDeleteUser: function (username) {
      var toDelete = this.collection.findWhere({user: username});
      toDelete.destroy({wait: true});

      window.App.navigate('#users', {trigger: true});
    },

    submitEditCurrentUserProfile: function () {
      var name = $('#editCurrentName').val();
      var img = $('#editCurrentUserProfileImg').val();
      img = this.parseImgString(img);

      var callback = function (error) {
        if (error) {
          arangoHelper.arangoError('User', 'Could not edit user settings');
        } else {
          arangoHelper.arangoNotification('User', 'Changes confirmed.');
          this.updateUserProfile();
        }
      }.bind(this);

      this.currentUser.setExtras(name, img, callback);
      window.modalView.hide();
    },

    submitEditUserPassword: function () {
      var newPasswd = $('#newCurrentPassword').val();
      var confirmPasswd = $('#confirmCurrentPassword').val();
      $('#newCurrentPassword').val('');
      $('#confirmCurrentPassword').val('');
      // check input
      // clear all "errors"
      $('#newCurrentPassword').closest('th').css('backgroundColor', 'white');
      $('#confirmCurrentPassword').closest('th').css('backgroundColor', 'white');

      // check
      var hasError = false;

      // check confirmation
      if (newPasswd !== confirmPasswd) {
        arangoHelper.arangoError('User', 'New passwords do not match.');
        hasError = true;
      }

      if (!hasError) {
        this.currentUser.setPassword(newPasswd);
        arangoHelper.arangoNotification('User', 'Password changed.');
        window.modalView.hide();
      }
    },

    validateUsername: function (username) {
      if (username === '') {
        arangoHelper.arangoError('You have to define an username');
        $('#newUsername').closest('th').css('backgroundColor', 'red');
        return false;
      }
      if (!username.match(/^[a-zA-Z][a-zA-Z0-9_\-]*$/)) {
        arangoHelper.arangoError(
          'Wrong Username', 'Username may only contain numbers, letters, _ and -'
        );
        return false;
      }
      return true;
    },

    editUserPassword: function () {
      window.modalView.hide();
      this.createEditUserPasswordModal();
    },

    validateName: function (name) {
      if (name === '') {
        return true;
      }
      if (!name.match(/^[a-zA-Z][a-zA-Z0-9_\-\ ]*$/)) {
        arangoHelper.arangoError(
          'Wrong Username', 'Username may only contain numbers, letters, _ and -'
        );
        return false;
      }
      return true;
    },

    submitEditUser: function (username) {
      var name = $('#editName').val();
      var status = $('#editStatus').is(':checked');

      if (!this.validateStatus(status)) {
        $('#editStatus').closest('th').css('backgroundColor', 'red');
        return;
      }
      if (!this.validateName(name)) {
        $('#editName').closest('th').css('backgroundColor', 'red');
        return;
      }
      var user = this.collection.findWhere({'user': username});
      user.save({'extra': {'name': name}, 'active': status}, {
        type: 'PATCH',
        success: function () {
          arangoHelper.arangoNotification('User', user.get('user') + ' updated.');
        },
        error: function () {
          arangoHelper.arangoError('User', 'Could not update ' + user.get('user') + '.');
        }
      });
    },

    breadcrumb: function () {
      $('#subNavigationBar .breadcrumb').html(
        'User: ' + this.username
      );
    }

  });
}());
