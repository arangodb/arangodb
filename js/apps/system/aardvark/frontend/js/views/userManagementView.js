/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, document, Backbone, EJS, SwaggerUi, hljs, $, arangoHelper, templateEngine */
(function() {

  "use strict";

  window.userManagementView = Backbone.View.extend({
    el: '#content',
    el2: '#userManagementThumbnailsIn',


    template: templateEngine.createTemplate("userManagementView.ejs"),

    events: {
      "click #createUser"                   : "createUser",
      "click #submitCreateUser"             : "submitCreateUser",
      "click #deleteUser"                   : "removeUser",
      "click #submitDeleteUser"             : "submitDeleteUser",
      "click .editUser"                     : "editUser",
      "click .icon"                         : "editUser",
      "click #submitEditUser"               : "submitEditUser",
      "click #userManagementToggle"         : "toggleView",
      "keyup #userManagementSearchInput"    : "search",
      "click #userManagementSearchSubmit"   : "search",
      "click #callEditUserPassword"         : "editUserPassword",
      "click #submitEditUserPassword"       : "submitEditUserPassword",
      "click #submitEditCurrentUserProfile" : "submitEditCurrentUserProfile"

    },

    initialize: function() {
      //fetch collection defined in router
      this.collection.fetch({async:false});
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
        $('#editCurrentUserProfileModal').modal('show');
      }

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
      //reset modal (e.g. if opened, filled in and closed its not empty)
      $('#newUsername').val('');
      $('#newName').val('');
      $('#newPassword').val('');
      $('#newStatus').prop('checked', true);

      this.showModal();
    },

    showModal: function() {
      $('#createUserModal').modal('show');
    },

    hideModal: function() {
      $('#createUserModal').modal('hide');
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
          self.handleError(err.status, err.statusText, name);
        },
        success: function(data) {
          self.hideModal();
          self.updateUserManagement();
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

    removeUser : function(e) {
      $('#editUserModal').modal('hide');
      this.userToDelete = $("#editUsername").html();
      $('#deleteUserModal').modal('show');
      e.stopPropagation();
    },

    submitDeleteUser : function(e) {
      var toDelete = this.collection.findWhere({user: this.userToDelete});
      toDelete.destroy({wait: true});
//      arangoHelper.arangoNotification("User " + this.userToDelete + " deleted.");
      this.userToDelete = '';
      $('#deleteUserModal').modal('hide');
      this.updateUserManagement();
    },

    editUser : function(e) {
      this.currentUser = this.collection.findWhere({user: this.collection.whoAmI()});
      this.collection.fetch();
      this.userToEdit = this.evaluateUserName($(e.currentTarget).attr("id"), '_edit-user');
      if (this.userToEdit === '') {
        this.userToEdit = $(e.currentTarget).attr('id');
      }
      var user = this.collection.findWhere({user: this.userToEdit});
      if (user.get("loggedIn")) {
        $('#editCurrentUserProfileModal').modal('show');
        $('#editCurrentUsername').html(user.get("user"));
        $('#editCurrentName').val(user.get("extra").name);
        $('#editCurrentUserProfileImg').val(user.get("extra").img);
      } else {
        $('#editUserModal').modal('show');
        $('#editUsername').html(user.get("user"));
        $('#editName').val(user.get("extra").name);
        $('#editStatus').attr("disabled", false);
        $('#deleteUser').attr("disabled", false);
        $('#deleteUser').removeClass("button-inactive");
        $('#deleteUser').addClass("button-danger");
      }
    },

    submitEditUser : function() {
      var self = this;
      var userName      = this.userToEdit;
      var name          = $('#editName').val();
      var status        = $('#editStatus').is(':checked');
      if (!this.validateStatus(status)) {
        $('#editStatus').closest("th").css("backgroundColor", "red");
        return;
      }
      if (!this.validateName(name)) {
        $('#editName').closest("th").css("backgroundColor", "red");
        return;
      }
      var user = this.collection.findWhere({"user":this.userToEdit});
      //img may not be set, so keep the value
      var img = user.get("extra").img;
      user.set({"extra": {"name":name, "img":img}, "active":status});
      user.save();
      this.userToEdit = '';
      $('#editUserModal').modal('hide');
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
      var index = str.lastIndexOf(substr);
      return str.substring(0, index);
    },


    editUserPassword : function () {
      $('#editCurrentUserProfileModal').modal('hide');
      $('#editUserPasswordModal').modal('show');
    },

    submitEditUserPassword : function () {
      var self        = this,
        oldPasswd     = $('#oldCurrentPassword').val(),
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
      //Check old password
      if (!this.validateCurrentPassword(oldPasswd)) {
        $('#oldCurrentPassword').closest("th").css("backgroundColor", "red");
        hasError = true;
      }
      //check confirmation
      if (newPasswd !== confirmPasswd) {
        $('#confirmCurrentPassword').closest("th").css("backgroundColor", "red");
        hasError = true;
      }
      //check new password
      if (!this.validatePassword(newPasswd)) {
        $('#newCurrentPassword').closest("th").css("backgroundColor", "red");
        hasError = true;
      }

      if (hasError) {
        return;
      }
      this.currentUser.setPassword(newPasswd);
//      this.showModal('#editUserProfileModal');
      $('#editUserPasswordModal').modal('hide');
    },

    validateCurrentPassword : function (pwd) {
      return this.currentUser.checkPassword(pwd);
    },


    submitEditCurrentUserProfile: function() {
      var self = this;
      var name      = $('#editCurrentName').val();
      var img       = $('#editCurrentUserProfileImg').val();

      img = this.parseImgString(img);
      /*      if (!this.validateName(name)) {
       $('#editName').closest("th").css("backgroundColor", "red");
       return;
       }*/

      this.currentUser.save({"extra": {"name":name, "img":img}});
      $('#editCurrentUserProfileModal').modal('hide');
      this.updateUserProfile();
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
    }


  });
}());
