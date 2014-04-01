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
      "click #submitEditUserProfile" : "submitEditUserProfile"
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

      $("[data-toggle=tooltip]").tooltip();

      $('.modalInfoTooltips').tooltip({
        placement: "left"
      });

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



    showModal: function(dialog) {
      $(dialog).modal('show');
    },

    hideModal: function(dialog) {
      $(dialog).modal('hide');
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
