/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, document, Backbone, EJS, SwaggerUi, hljs, $, arangoHelper, templateEngine */
(function() {

  "use strict";

  window.userManagementView = Backbone.View.extend({
    el: '#content',
    el2: '#userManagementThumbnailsIn',


    template: templateEngine.createTemplate("userManagementView.ejs"),

    events: {
      "click #createUser"                                     : "createUser",
      "click #submitCreateUser"                               : "submitCreateUser",
      "click #deleteUser"                                     : "removeUser",
      "click #submitDeleteUser"                               : "submitDeleteUser",
      "click .editUser"                                       : "editUser",
      "click .icon"                                           : "editUser",
      "click #submitEditUser"                                 : "submitEditUser",
      "click #userManagementToggle"                           : "toggleView",
      "keyup #userManagementSearchInput"                      : "search",
      "click #userManagementSearchSubmit"                     : "search"
    },

    initialize: function() {
      //fetch collection defined in router
      this.collection.fetch({async:false});
    },

    render: function () {
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
      this.collection.fetch();
      this.userToEdit = this.evaluateUserName($(e.currentTarget).attr("id"), '_edit-user');
      $('#editUserModal').modal('show');
      var user = this.collection.findWhere({user: this.userToEdit});
      $('#editUsername').html(user.get("user"));
      $('#editName').val(user.get("extra").name);
      $('#editStatus').attr("checked", user.get("active"));
      if (user.get("loggedIn")) {
        $('#editStatus').attr("disabled", true);
        $('#deleteUser').attr("disabled", true);
        $('#deleteUser').removeClass("button-danger");
        $('#deleteUser').addClass("button-inactive");
      } else {
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
    }




  });
}());
