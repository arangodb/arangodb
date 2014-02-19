/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, document, Backbone, EJS, SwaggerUi, hljs, $, arangoHelper, templateEngine */
(function() {

  "use strict";

  window.userManagementView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate("userManagementView.ejs"),

    events: {
      "click #createUser"                                     : "createUser",
      "click #submitCreateUser"                               : "submitCreateUser",
      "click #userManagementTable .icon_arangodb_roundminus"  : "removeUser",
      "click #submitDeleteUser"                               : "submitDeleteUser",
//      "click .editUser"                                       : "editUser",
      "click #submitEditUser"                                 : "submitEditUser"
    },

    initialize: function() {
      //fetch collection defined in router
      this.collection.fetch({async:false});
    },

    render: function(){
      $(this.el).html(this.template.render({
        collection: this.collection
      }));
      this.renderTable();
      //this.selectCurrentUser(); //selectCurrentUser not implemented yet
      return this;
    },

    renderTable: function () {
      this.collection.forEach(function(user) {
        $("#userManagementTable tbody").append(
          '<tr class="editUser" id="' + user.get("user") + '">' +
            '<td><a>' + user.get("user") + '</a></td>' +//username
            '<td><a>' + "" + '</a></td>' +//avatar
            '<td><a>' + "" + '</a></td>' +//name
            '<td><a>' + user.get("active") + '</a></td>' +//active
            '<td><span class="arangoicon icon_arangodb_roundminus"' +
            'data-original-title="Delete user"></span></td>' +
          '</tr>'
        );
      });
    },

    selectCurrentUser : function() {
      $('#userManagementTableBody tr').addClass('userInactive');
      var tr = $('#userManagementTableBody td:contains('+this.currentUser+')').parent();
      $(tr).removeClass('userInactive').addClass('userActive');

    },

    createUser : function() {
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
        username: userName,
        passwd: userPassword,
        active: status
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
      this.userToDelete = $(e.currentTarget).parent().parent().children().first().text();
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
      this.userToEdit = $(e.currentTarget).attr("id");
      console.log(this.userToEdit);
      $('#editUserModal').modal('show');
      var user = this.collection.findWhere({user: this.userToEdit});
      $('#editUsername').html(user.get("user"));
//      $('#editName').val(user.get("user"));
      $('#editStatus').attr("checked", user.get("active"));
    },

    submitEditUser : function() {
      console.log("submitEditUser");
      var self = this;
      var userName      = this.userToEdit;
      var name          = $('#editName').val();
      var status        = $('#editStatus').is(':checked');
      console.log(userName);
      console.log(name);
      console.log(status);
      if (!this.validateStatus(status)) {
        $('#editStatus').closest("th").css("backgroundColor", "red");
        return;
      }
      if (!this.validateName(name)) {
        $('#editName').closest("th").css("backgroundColor", "red");
        return;
      }
      var options = {
        username: userName,
        //name: name,
        active: status
      };
      //TODO
      this.collection.set(options, {
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

    validateUsername: function (username) {
      if (username === "") {
        arangoHelper.arangoError("You have to define an username");
        $('#newUsername').closest("th").css("backgroundColor", "red");
        return false;
      }
      if (!username.match(/^[a-zA-Z][a-zA-Z0-9_\-]*$/)) {
        arangoHelper.arangoError("Username may only contain numbers, letters, _ and -");
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
      if (!name.match(/^[a-zA-Z][a-zA-Z0-9_\-]*$/)) {
        arangoHelper.arangoError("Username may only contain numbers, letters, _ and -");
        return false;
      }
      return true;
    },

    validateStatus: function (status) {
      if (status === "") {
        return false;
      }
      return true;
    }

  });
}());
