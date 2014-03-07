/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, document, Backbone, EJS, SwaggerUi */
/*global hljs, $, arangoHelper, templateEngine, CryptoJS */
(function() {

  "use strict";

  window.userProfileView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate("userProfileView.ejs"),

    events: {
      "click #profileContent" : "editUserProfile",
      "click #callEditUserPassword" : "editUserPassword",
      "click #submitEditUserProfile" : "submitEditUserProfile",
      "click #submitEditUserPassword" : "submitEditUserPassword"
    },

    initialize: function() {
      this.collection.fetch({async:false});
      this.user = this.collection.findWhere({loggedIn: true});
    },

    render: function(){
      var img = this.getAvatarSource(this.user.get("extra").img);
      $(this.el).html(this.template.render({
        img : img,
        name : this.user.get("extra").name,
        username : this.user.get("user")

      }));
      return this;
    },

    editUserProfile : function() {
      this.collection.fetch();
      $('#editUsername').html(this.user.get("user"));
      $('#editName').val(this.user.get("extra").name);
      $('#editUserProfileImg').val(this.user.get("extra").img);

      this.showModal('#editUserProfileModal');
    },

    submitEditUserProfile : function() {
      var self = this;
      var name      = $('#editName').val();
      var img       = $('#editUserProfileImg').val();

      img = this.parseImgString(img);
/*      if (!this.validateName(name)) {
        $('#editName').closest("th").css("backgroundColor", "red");
        return;
      }*/

      this.user.save({"extra": {"name":name, "img":img}});
      this.hideModal('#editUserProfileModal');
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

    editUserPassword : function () {
      this.hideModal('#editUserProfileModal');
      this.showModal('#editUserPasswordModal');
    },

    submitEditUserPassword : function () {
      var self        = this,
        oldPasswd     = $('#oldPassword').val(),
        newPasswd     = $('#newPassword').val(),
        confirmPasswd = $('#confirmPassword').val();
      $('#oldPassword').val('');
      $('#newPassword').val('');
      $('#confirmPassword').val('');

      //check input
      //clear all "errors"
      $('#oldPassword').closest("th").css("backgroundColor", "white");
      $('#newPassword').closest("th").css("backgroundColor", "white");
      $('#confirmPassword').closest("th").css("backgroundColor", "white");


      //check
      var hasError = false;
      //Check old password
      if (!this.validateCurrentPassword(oldPasswd)) {
        $('#oldPassword').closest("th").css("backgroundColor", "red");
        hasError = true;
      }
      //check confirmation
      if (newPasswd !== confirmPasswd) {
        $('#confirmPassword').closest("th").css("backgroundColor", "red");
        hasError = true;
      }
      //check new password
      if (!this.validatePassword(newPasswd)) {
        $('#newPassword').closest("th").css("backgroundColor", "red");
        hasError = true;
      }

      if (hasError) {
        return;
      }
      this.user.setPassword(newPasswd);
//      this.showModal('#editUserProfileModal');
      this.hideModal('#editUserPasswordModal');
    },


    showModal: function(dialog) {
      $(dialog).modal('show');
    },

    hideModal: function(dialog) {
      $(dialog).modal('hide');
    },

    parseImgString : function(img) {
      //if already md5
      if (img.indexOf("@") === -1) {
        return img;
      }
      //else generate md5
      return CryptoJS.MD5(img).toString();
    },

    validateCurrentPassword : function (pwd) {
      return this.user.checkPassword(pwd);
    },

    validatePassword : function (pwd) {
        return true;
    },

    getAvatarSource: function(img) {
      var result = '<img src="';
      if(img) {
        result += 'https://s.gravatar.com/avatar/';
        result += img;
        result += '?s=150';
      } else {
        result += 'img/arangodblogoAvatar_150.png';
      }
      result += '" />';
      return result;
    }

  });
}());
