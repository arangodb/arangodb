/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, document, Backbone, EJS, SwaggerUi, hljs, $, arangoHelper, templateEngine */
(function() {

  "use strict";

  window.userProfileView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate("userProfileView.ejs"),

    events: {
      "click #profileContent" : "editUserProfile",
      "click #submitEditUserProfile" : "submitEditUserProfile"
    },

    initialize: function() {
      this.collection.fetch({async:false});
      this.user = this.collection.findWhere({loggedIn: true});

    },

    render: function(){
      $(this.el).html(this.template.render({
        img : this.user.get("extra").img,
        name : this.user.get("extra").name,
        username : this.user.get("user")

      }));
      return this;
    },

    editUserProfile : function() {
      this.collection.fetch();
      $('#editUsername').html(this.user.get("user"));
      $('#editName').val(this.user.get("extra").name);
      $('#editImg').val(this.user.get("extra").img);

      this.showModal();
    },

    submitEditUserProfile : function() {
      var self = this;
      var userName  = this.user.get("user");
      var name      = $('#editName').val();
      var img       = $('#editImg').val();
      var active    = this.user.get("active");

      img = this.parseImgString(img);
/*      if (!this.validateName(name)) {
        $('#editName').closest("th").css("backgroundColor", "red");
        return;
      }*/
      this.user.set({"extra": {"name":name, "img":img}, "active":active});
      this.user.save();
      this.hideModal();
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


    showModal: function() {
      $('#editUserProfileModal').modal('show');
    },

    hideModal: function() {
      $('#editUserProfileModal').modal('hide');
    },

    parseImgString : function(img) {
      var strings;
      if (img.search("avatar/") !== -1) {
        strings = img.split("avatar/");
        img = strings[1];
      }
      if (img.search("\\?") !== -1) {
        strings = img.split("?");
        img = strings[0];
      }
      return img;
    }



  });
}());
