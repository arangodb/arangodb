/* jshint browser: true */
/* jshint unused: false */
/* global frontendConfig, window, Backbone, $, arangoHelper, templateEngine, Joi */
(function () {
  'use strict';

  window.UserManagementView = Backbone.View.extend({
    el: '#content',
    el2: '#userManagementThumbnailsIn',

    template: templateEngine.createTemplate('userManagementView.ejs'),

    remove: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    events: {
      'click #createUser': 'createUser',
      'click #submitCreateUser': 'submitCreateUser',
      'click #userManagementThumbnailsIn .tile': 'editUser',
      'click #submitEditUser': 'submitEditUser',
      'click #userManagementToggle': 'toggleView',
      'keyup #userManagementSearchInput': 'search',
      'click #userManagementSearchSubmit': 'search',
      'click #callEditUserPassword': 'editUserPassword',
      'click #submitEditUserPassword': 'submitEditUserPassword',
      'click #submitEditCurrentUserProfile': 'submitEditCurrentUserProfile',
      'click .css-label': 'checkBoxes',
      'change #userSortDesc': 'sorting'

    },

    dropdownVisible: false,

    initialize: function () {
      var self = this;
      var callback = function (error, user) {
        if (frontendConfig.authenticationEnabled === true) {
          if (error || user === null) {
            arangoHelper.arangoError('User', 'Could not fetch user data');
          } else {
            this.currentUser = this.collection.findWhere({user: user});
          }
        }
      }.bind(this);

      // fetch collection defined in router
      this.collection.fetch({
        fetchAllUsers: true,
        cache: false,
        success: function () {
          self.collection.whoAmI(callback);
        }
      });
    },

    checkBoxes: function (e) {
      // chrome bugfix
      var clicked = e.currentTarget.id;
      $('#' + clicked).click();
    },

    sorting: function () {
      if ($('#userSortDesc').is(':checked')) {
        this.collection.setSortingDesc(true);
      } else {
        this.collection.setSortingDesc(false);
      }

      if ($('#userManagementDropdown').is(':visible')) {
        this.dropdownVisible = true;
      } else {
        this.dropdownVisible = false;
      }

      this.render();
    },

    render: function (isProfile) {
      var dropdownVisible = false;
      if ($('#userManagementDropdown').is(':visible')) {
        dropdownVisible = true;
      }

      var callbackFunction = function () {
        this.collection.sort();
        $(this.el).html(this.template.render({
          collection: this.collection,
          searchString: ''
        }));

        if (dropdownVisible === true) {
          $('#userManagementDropdown2').show();
          $('#userSortDesc').attr('checked', this.collection.sortOptions.desc);
          $('#userManagementToggle').toggleClass('activated');
          $('#userManagementDropdown').show();
        }

        if (isProfile) {
          this.editCurrentUser();
        }

        arangoHelper.setCheckboxStatus('#userManagementDropdown');
      }.bind(this);

      this.collection.fetch({
        fetchAllUsers: true,
        cache: false,
        success: function () {
          callbackFunction();
        }
      });

      return this;
    },

    search: function () {
      var searchInput,
        searchString,
        strLength,
        reducedCollection;

      searchInput = $('#userManagementSearchInput');
      searchString = arangoHelper.escapeHtml($('#userManagementSearchInput').val());
      reducedCollection = this.collection.filter(
        function (u) {
          return u.get('user').indexOf(searchString) !== -1;
        }
      );
      $(this.el).html(this.template.render({
        collection: reducedCollection,
        searchString: searchString
      }));

      // after rendering, get the "new" element
      searchInput = $('#userManagementSearchInput');
      // set focus on end of text in input field
      strLength = searchInput.val().length;
      searchInput.focus();
      searchInput[0].setSelectionRange(strLength, strLength);
    },

    createUser: function (e) {
      e.preventDefault();
      this.createCreateUserModal();
    },

    submitCreateUser: function () {
      var self = this;
      var userName = $('#newUsername').val();
      var name = $('#newName').val();
      var userPassword = $('#newPassword').val();
      var status = $('#newStatus').is(':checked');
      if (!this.validateUserInfo(name, userName, userPassword, status)) {
        return;
      }
      var options = {
        user: userName,
        passwd: userPassword,
        active: status,
        extra: {name: name}
      };

      if (frontendConfig.isEnterprise && $('#newRole').is(':checked')) {
        options.user = ':role:' + userName;
        delete options.passwd;
      }

      this.collection.create(options, {
        wait: true,
        error: function (data, err) {
          arangoHelper.parseError('User', err, data);
        },
        success: function () {
          self.updateUserManagement();
          window.modalView.hide();
        }
      });
    },

    validateUserInfo: function (name, username, pw, status) {
      if (username === '') {
        arangoHelper.arangoError('You have to define an username');
        $('#newUsername').closest('th').css('backgroundColor', 'red');
        return false;
      }
      return true;
    },

    updateUserManagement: function () {
      var self = this;
      this.collection.fetch({
        fetchAllUsers: true,
        cache: false,
        success: function () {
          self.render();
        }
      });
    },

    editUser: function (e) {
      if ($(e.currentTarget).find('a').attr('id') === 'createUser') {
        return;
      }

      if ($(e.currentTarget).hasClass('tile')) {
        if ($(e.currentTarget).find('.fa').attr('id')) {
          e.currentTarget = $(e.currentTarget).find('.fa');
        } else {
          // check if gravatar icon is enabled
          e.currentTarget = $(e.currentTarget).find('.icon');
        }
      }

      this.collection.fetch({
        fetchAllUsers: true,
        cache: false
      });
      var username = this.evaluateUserName($(e.currentTarget).attr('id'), '_edit-user');
      if (username === '') {
        username = $(e.currentTarget).attr('id');
      }

      window.App.navigate('user/' + encodeURIComponent(username), {trigger: true});
    },

    toggleView: function () {
      // apply sorting to checkboxes
      $('#userSortDesc').attr('checked', this.collection.sortOptions.desc);

      $('#userManagementToggle').toggleClass('activated');
      $('#userManagementDropdown2').slideToggle(200);
    },

    createCreateUserModal: function () {
      var buttons = [];
      var tableContent = [];

      tableContent.push(
        window.modalView.createTextEntry(
          'newUsername',
          'Username',
          '',
          false,
          'Username',
          true,
          [
            {
              rule: Joi.string().regex(/^[a-zA-Z0-9\-_]*$/),
              msg: 'Only symbols, "_" and "-" are allowed.'
            },
            {
              rule: Joi.string().required(),
              msg: 'No username given.'
            }
          ]
        )
      );
      tableContent.push(
        window.modalView.createTextEntry('newName', 'Name', '', false, 'Name', false)
      );
      tableContent.push(
        window.modalView.createPasswordEntry('newPassword', 'Password', '', false, '', false)
      );
      if (frontendConfig.isEnterprise) {
        tableContent.push(
          window.modalView.createCheckboxEntry('newRole', 'Role', false, false, false)
        );
      }
      tableContent.push(
        window.modalView.createCheckboxEntry('newStatus', 'Active', 'active', false, true)
      );
      buttons.push(
        window.modalView.createSuccessButton('Create', this.submitCreateUser.bind(this))
      );

      window.modalView.show('modalTable.ejs', 'Create New User', buttons, tableContent);

      if (frontendConfig.isEnterprise) {
        $('#newRole').on('change', function () {
          if ($('#newRole').is(':checked')) {
            $('#newPassword').attr('disabled', true);
          } else {
            $('#newPassword').attr('disabled', false);
          }
        });
      }
    },

    evaluateUserName: function (str, substr) {
      if (str) {
        var index = str.lastIndexOf(substr);
        return str.substring(0, index);
      }
    },

    updateUserProfile: function () {
      var self = this;
      this.collection.fetch({
        fetchAllUsers: true,
        cache: false,
        success: function () {
          self.render();
        }
      });
    }

  });
}());
