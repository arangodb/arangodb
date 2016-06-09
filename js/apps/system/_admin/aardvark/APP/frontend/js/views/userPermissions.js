/*jshint browser: true */
/*jshint unused: false */
/*global CryptoJS, _, arangoHelper, Backbone, window, templateEngine, $ */

(function() {
  "use strict";

  window.UserPermissionView = Backbone.View.extend({

    el: "#content",

    template: templateEngine.createTemplate("userPermissionView.ejs"),

    initialize: function(options) {
      this.username = options.username;
    },

    events: {
      'click #userPermissionView [type="checkbox"]': 'setPermission'
    },

    render: function() {
      var self = this;

      this.collection.fetch({
        success: function() {
          self.continueRender();
        }
      });

    },

    setPermission: function(e) {
      var checked = $(e.currentTarget).is(':checked');
      var db = $(e.currentTarget).attr('name');
      
      if (checked) {
         this.grantPermission(this.currentUser.get("user"), db);
      }
      else {
        this.revokePermission(this.currentUser.get("user"), db);
      }
    },

    grantPermission: function(user, db) {
      $.ajax({
        type: "PUT",
        url: arangoHelper.databaseUrl("/_api/user/" + encodeURIComponent(user) + "/database/" + encodeURIComponent(db)),
        contentType: "application/json",
        data: JSON.stringify({
          grant: 'rw'
        })
      });
    },

    revokePermission: function(user, db) {
      $.ajax({
        type: "PUT",
        url: arangoHelper.databaseUrl("/_api/user/" + encodeURIComponent(user) + "/database/" + encodeURIComponent(db)),
        contentType: "application/json",
        data: JSON.stringify({
          grant: ''
        })
      });
    },

    continueRender: function() {
      var self = this;
      
      this.currentUser = this.collection.findWhere({
        user: this.username
      });

      this.breadcrumb();

      arangoHelper.buildUserSubNav(this.currentUser.get("user"), 'Permissions');

      //FETCH COMPLETE DB LIST
      $.ajax({
        type: "GET",
        //url: arangoHelper.databaseUrl("/_api/user/" + encodeURIComponent(this.currentUser.get("user")) + "/config"),
        url: arangoHelper.databaseUrl("/_api/database/user"),
        contentType: "application/json",
        success: function(data) {
          var allDBs = data.result;

          //NOW FETCH USER PERMISSIONS
          $.ajax({
            type: "GET",
            url: arangoHelper.databaseUrl("/_api/user/" + encodeURIComponent(self.currentUser.get("user")) + "/permission"),
            //url: arangoHelper.databaseUrl("/_api/database/user/" + encodeURIComponent(this.currentUser.get("user"))),
            contentType: "application/json",
            success: function(data) {
              var permissions = data.result;
              self.finishRender(allDBs, permissions);
            }
          });

        }
      });
    },

    finishRender: function(allDBs, permissions) {

      $(this.el).html(this.template.render({
        allDBs: allDBs, 
        permissions: permissions
      }));

    },

    breadcrumb: function () {
      $('#subNavigationBar .breadcrumb').html(
        'User: ' + this.currentUser.get("user")
      );
    }

  });

}());
