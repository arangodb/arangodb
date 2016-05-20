/*jshint browser: true */
/*jshint unused: false */
/*global frontendConfig, _, window, document, Backbone, EJS, SwaggerUi, hljs, $, arangoHelper, templateEngine,
  CryptoJS, Joi */
(function() {

  "use strict";

  window.userManagementView = Backbone.View.extend({
    el: '#content',
    el2: '#userManagementThumbnailsIn',

    template: templateEngine.createTemplate("userManagementView.ejs"),

    events: {
      "click #createUser"                       : "createUser",
      "click #submitCreateUser"                 : "submitCreateUser",
//      "click #deleteUser"                   : "removeUser",
//      "click #submitDeleteUser"             : "submitDeleteUser",
//      "click .editUser"                     : "editUser",
//      "click .icon"                         : "editUser",
      "click #userManagementThumbnailsIn .tile" : "editUser",
      "click #submitEditUser"                   : "submitEditUser",
      "click #userManagementToggle"             : "toggleView",
      "keyup #userManagementSearchInput"        : "search",
      "click #userManagementSearchSubmit"       : "search",
      "click #callEditUserPassword"             : "editUserPassword",
      "click #submitEditUserPassword"           : "submitEditUserPassword",
      "click #submitEditCurrentUserProfile"     : "submitEditCurrentUserProfile",
      "click .css-label"                        : "checkBoxes",
      "change #userSortDesc"                    : "sorting"

    },

    dropdownVisible: false,

    initialize: function() {
      var self = this,
      callback = function(error, user) {
        if (frontendConfig.authenticationEnabled === true) {
          if (error || user === null) {
            arangoHelper.arangoError("User", "Could not fetch user data");
          }
          else {
            this.currentUser = this.collection.findWhere({user: user});
          }
        }
      }.bind(this);

      //fetch collection defined in router
      this.collection.fetch({
        success: function() {
          self.collection.whoAmI(callback);
        }
      });
    },

    checkBoxes: function (e) {
      //chrome bugfix
      var clicked = e.currentTarget.id;
      $('#'+clicked).click();
    },

    sorting: function() {
      if ($('#userSortDesc').is(":checked")) {
        this.collection.setSortingDesc(true);
      }
      else {
        this.collection.setSortingDesc(false);
      }

      if ($('#userManagementDropdown').is(":visible")) {
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

      this.collection.sort();
      $(this.el).html(this.template.render({
        collection   : this.collection,
        searchString : ''
      }));
//      this.setFilterValues();

      if (dropdownVisible === true) {
        $('#userManagementDropdown2').show();
        $('#userSortDesc').attr('checked', this.collection.sortOptions.desc);
        $('#userManagementToggle').toggleClass('activated');
        $('#userManagementDropdown').show();
      }

//      var searchOptions = this.collection.searchOptions;

      //************
/*      this.userCollection.getFiltered(searchOptions).forEach(function (arango_collection) {
        $('#userManagementThumbnailsIn', this.el).append(new window.CollectionListItemView({
          model: arango_collection
        }).render().el);
      }, this);
*/
/*      $('#searchInput').val(searchOptions.searchPhrase);
      $('#searchInput').focus();
      var val = $('#searchInput').val();
      $('#searchInput').val('');
      $('#searchInput').val(val);
*/
//      arangoHelper.fixTooltips(".icon_arangodb, .arangoicon", "left");
      if (!!isProfile) {
        this.editCurrentUser();
      }

      arangoHelper.setCheckboxStatus('#userManagementDropdown');

      return this;
    },

    search: function() {
      var searchInput,
        searchString,
        strLength,
        reducedCollection;

      searchInput = $('#userManagementSearchInput');
      searchString = $("#userManagementSearchInput").val();
      reducedCollection = this.collection.filter(
        function(u) {
          return u.get("user").indexOf(searchString) !== -1;
        }
      );
      $(this.el).html(this.template.render({
        collection   : reducedCollection,
        searchString : searchString
      }));

      //after rendering, get the "new" element
      searchInput = $('#userManagementSearchInput');
      //set focus on end of text in input field
      strLength= searchInput.val().length;
      searchInput.focus();
      searchInput[0].setSelectionRange(strLength, strLength);
    },

    createUser : function(e) {
      e.preventDefault();
      this.createCreateUserModal();
    },

    submitCreateUser: function() {
      var self = this;
      var userName      = $('#newUsername').val();
      var name          = $('#newName').val();
      var userPassword  = $('#newPassword').val();
      var status        = $('#newStatus').is(':checked');
      if (!this.validateUserInfo(name, userName, userPassword, status)) {
        return;
      }
      var options = {
        user: userName,
        passwd: userPassword,
        active: status,
        extra:{name: name}
      };
      this.collection.create(options, {
        wait:true,
        error: function(data, err) {
          arangoHelper.parseError("User", err, data);
        },
        success: function() {
          self.updateUserManagement();
          window.modalView.hide();
        }
      });
    },

    validateUserInfo: function (name, username, pw, status) {
      if (username === "") {
        arangoHelper.arangoError("You have to define an username");
        $('#newUsername').closest("th").css("backgroundColor", "red");
        return false;
      }
/*      if (!username.match(/^[a-zA-Z][a-zA-Z0-9_\-]*$/)) {
        arangoHelper.arangoError("Name may only contain numbers, letters, _ and -");
        return false;
      }
      if (!user.match(/^[a-zA-Z][a-zA-Z\-]*$/)) {
        arangoHelper.arangoError("Name may only letters and -");
        return false;
      }*/
      return true;
    },

    updateUserManagement: function() {
      var self = this;
      this.collection.fetch({
        success: function() {
          self.render();
//          window.App.handleSelectDatabase();
        }
      });
    },

    submitDeleteUser: function(username) {
      var toDelete = this.collection.findWhere({user: username});
      toDelete.destroy({wait: true});
      window.modalView.hide();
      this.updateUserManagement();
    },

    editUser : function(e) {

      if ($(e.currentTarget).find('a').attr('id') === 'createUser') {
        return;
      }

      if ($(e.currentTarget).hasClass('tile')) {
        e.currentTarget = $(e.currentTarget).find('img');
      }

      this.collection.fetch();
      var username = this.evaluateUserName($(e.currentTarget).attr("id"), '_edit-user');
      if (username === '') {
        username = $(e.currentTarget).attr('id');
      }
      var user = this.collection.findWhere({user: username});
      if (user.get("loggedIn")) {
        this.editCurrentUser();
      } else {
        this.createEditUserModal(
          user.get("user"),
          user.get("extra").name,
          user.get("active")
        );
      }
    },

    editCurrentUser: function() {
      this.createEditCurrentUserModal(
        this.currentUser.get("user"),
        this.currentUser.get("extra").name,
        this.currentUser.get("extra").img
      );
    },

    submitEditUser : function(username) {
      var name = $('#editName').val();
      var status = $('#editStatus').is(':checked');

      if (!this.validateStatus(status)) {
        $('#editStatus').closest("th").css("backgroundColor", "red");
        return;
      }
      if (!this.validateName(name)) {
        $('#editName').closest("th").css("backgroundColor", "red");
        return;
      }
      var user = this.collection.findWhere({"user": username});
      //img may not be set, so keep the value
//      var img = user.get("extra").img;
//      user.set({"extra": {"name":name, "img":img}, "active":status});
      user.save({"extra": {"name":name}, "active":status}, {type: "PATCH"});
      window.modalView.hide();
      this.updateUserManagement();
    },

    validateUsername: function (username) {
      if (username === "") {
        arangoHelper.arangoError("You have to define an username");
        $('#newUsername').closest("th").css("backgroundColor", "red");
        return false;
      }
      if (!username.match(/^[a-zA-Z][a-zA-Z0-9_\-]*$/)) {
        arangoHelper.arangoError(
          "Wrong Username", "Username may only contain numbers, letters, _ and -"
        );
        return false;
      }
      return true;
    },

    validatePassword: function (passwordw) {
      return true;
    },

    validateName: function (name) {
      if (name === "") {
        return true;
      }
      if (!name.match(/^[a-zA-Z][a-zA-Z0-9_\-\ ]*$/)) {
        arangoHelper.arangoError(
          "Wrong Username", "Username may only contain numbers, letters, _ and -"
        );
        return false;
      }
      return true;
    },

    validateStatus: function (status) {
      if (status === "") {
        return false;
      }
      return true;
    },

    toggleView: function() {
      //apply sorting to checkboxes
      $('#userSortDesc').attr('checked', this.collection.sortOptions.desc);

      $('#userManagementToggle').toggleClass("activated");
      $('#userManagementDropdown2').slideToggle(200);
    },

    setFilterValues: function () {
      /*
      var searchOptions = this.collection.searchOptions;
      $('#checkLoaded').attr('checked', searchOptions.includeLoaded);
      $('#checkUnloaded').attr('checked', searchOptions.includeUnloaded);
      $('#checkSystem').attr('checked', searchOptions.includeSystem);
      $('#checkEdge').attr('checked', searchOptions.includeEdge);
      $('#checkDocument').attr('checked', searchOptions.includeDocument);
      $('#sortName').attr('checked', searchOptions.sortBy !== 'type');
      $('#sortType').attr('checked', searchOptions.sortBy === 'type');
      $('#sortOrder').attr('checked', searchOptions.sortOrder !== 1);
      */
    },

    evaluateUserName : function(str, substr) {
      if (str) {
        var index = str.lastIndexOf(substr);
        return str.substring(0, index);
      }
    },

    editUserPassword : function () {
      window.modalView.hide();
      this.createEditUserPasswordModal();
    },

    submitEditUserPassword : function () {
      var oldPasswd   = $('#oldCurrentPassword').val(),
        newPasswd     = $('#newCurrentPassword').val(),
        confirmPasswd = $('#confirmCurrentPassword').val();
      $('#oldCurrentPassword').val('');
      $('#newCurrentPassword').val('');
      $('#confirmCurrentPassword').val('');
      //check input
      //clear all "errors"
      $('#oldCurrentPassword').closest("th").css("backgroundColor", "white");
      $('#newCurrentPassword').closest("th").css("backgroundColor", "white");
      $('#confirmCurrentPassword').closest("th").css("backgroundColor", "white");

      //check
      var hasError = false;

      var callback = function(error, data) {
        if (error) {
          arangoHelper.arangoError("User", "Could not verify old password");
        }
        else {
          if (data) {
            //check confirmation
            if (newPasswd !== confirmPasswd) {
              arangoHelper.arangoError("User", "New passwords do not match");
              hasError = true;
            }
            //check new password
            /*if (!this.validatePassword(newPasswd)) {
              $('#newCurrentPassword').closest("th").css("backgroundColor", "red");
              hasError = true;
            }*/

            if (!hasError) {
              this.currentUser.setPassword(newPasswd);
              arangoHelper.arangoNotification("User", "Password changed");
              window.modalView.hide();
            }
          }
        }
      }.bind(this);
      this.currentUser.checkPassword(oldPasswd, callback);
    },

    submitEditCurrentUserProfile: function() {
      var name    = $('#editCurrentName').val();
      var img     = $('#editCurrentUserProfileImg').val();
      img = this.parseImgString(img);


      var callback = function(error) {
        if (error) {
          arangoHelper.arangoError("User", "Could not edit user settings");
        }
        else {
          arangoHelper.arangoNotification("User", "Changes confirmed.");
          this.updateUserProfile();
        }
      }.bind(this);

      this.currentUser.setExtras(name, img, callback);
      window.modalView.hide();
    },

    updateUserProfile: function() {
      var self = this;
      this.collection.fetch({
        success: function() {
          self.render();
        }
      });
    },

    parseImgString : function(img) {
      //if already md5
      if (img.indexOf("@") === -1) {
        return img;
      }
      //else generate md5
      return CryptoJS.MD5(img).toString();
    },


    //modal dialogs

    createEditUserModal: function(username, name, active) {
      var buttons, tableContent;
      tableContent = [
        {
          type: window.modalView.tables.READONLY,
          label: "Username",
          value: _.escape(username)
        },
        {
          type: window.modalView.tables.TEXT,
          label: "Name",
          value: name,
          id: "editName",
          placeholder: "Name"
        },
        {
          type: window.modalView.tables.CHECKBOX,
          label: "Active",
          value: "active",
          checked: active,
          id: "editStatus"
        }
      ];
      buttons = [
        {
          title: "Delete",
          type: window.modalView.buttons.DELETE,
          callback: this.submitDeleteUser.bind(this, username)
        },
        {
          title: "Save",
          type: window.modalView.buttons.SUCCESS,
          callback: this.submitEditUser.bind(this, username)
        }
      ];
      window.modalView.show("modalTable.ejs", "Edit User", buttons, tableContent);
    },

    createCreateUserModal: function() {
      var buttons = [],
        tableContent = [];

      tableContent.push(
        window.modalView.createTextEntry(
          "newUsername",
          "Username",
          "",
          false,
          "Username",
          true,
          [
            {
              rule: Joi.string().regex(/^[a-zA-Z0-9\-_]*$/),
              msg: 'Only symbols, "_" and "-" are allowed.'
            },
            {
              rule: Joi.string().required(),
              msg: "No username given."
            }
          ]
        )
      );
      tableContent.push(
        window.modalView.createTextEntry("newName", "Name", "", false, "Name", false)
      );
      tableContent.push(
        window.modalView.createPasswordEntry("newPassword", "Password", "", false, "", false)
      );
      tableContent.push(
        window.modalView.createCheckboxEntry("newStatus", "Active", "active", false, true)
      );
      buttons.push(
        window.modalView.createSuccessButton("Create", this.submitCreateUser.bind(this))
      );

      window.modalView.show("modalTable.ejs", "Create New User", buttons, tableContent);
    },

    createEditCurrentUserModal: function(username, name, img) {
      var buttons = [],
        tableContent = [];

      tableContent.push(
        window.modalView.createReadOnlyEntry("id_username", "Username", username)
      );
      tableContent.push(
        window.modalView.createTextEntry("editCurrentName", "Name", name, false, "Name", false)
      );
      tableContent.push(
        window.modalView.createTextEntry(
          "editCurrentUserProfileImg",
          "Gravatar account (Mail)",
          img,
          "Mailaddress or its md5 representation of your gravatar account."
            + " The address will be converted into a md5 string. "
            + "Only the md5 string will be stored, not the mailaddress.",
          "myAccount(at)gravatar.com"
        )
      );

      buttons.push(
        window.modalView.createNotificationButton(
          "Change Password",
          this.editUserPassword.bind(this)
        )
      );
      buttons.push(
        window.modalView.createSuccessButton(
          "Save",
          this.submitEditCurrentUserProfile.bind(this)
        )
      );

      window.modalView.show("modalTable.ejs", "Edit User Profile", buttons, tableContent);
    },

    createEditUserPasswordModal: function() {
      var buttons = [],
        tableContent = [];

      tableContent.push(
        window.modalView.createPasswordEntry(
          "oldCurrentPassword",
          "Old Password",
          "",
          false,
          "old password",
          false
        )
      );
      tableContent.push(
        window.modalView.createPasswordEntry(
          "newCurrentPassword",
          "New Password",
          "",
          false,
          "new password",
          false
        )
      );
      tableContent.push(
        window.modalView.createPasswordEntry(
          "confirmCurrentPassword",
          "Confirm New Password",
          "",
          false,
          "confirm new password",
          false)
      );

      buttons.push(
        window.modalView.createSuccessButton(
          "Save",
          this.submitEditUserPassword.bind(this)
        )
      );

      window.modalView.show("modalTable.ejs", "Edit User Password", buttons, tableContent);
    }

  });
}());
