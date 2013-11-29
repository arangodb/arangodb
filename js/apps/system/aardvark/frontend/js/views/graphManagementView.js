/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, $, _, window, templateEngine*/

(function() {
  "use strict";

  window.GraphManagementView = Backbone.View.extend({
    el: '#content',
    template: templateEngine.createTemplate("manageGraphsView.ejs"),

    events: {
      "click .deleteGraph": "deleteGraph",
      "click #addGraphButton": "addNewGraph"
    },

    addNewGraph: function() {
      window.App.navigate("graphManagement/add", {trigger: true});
    },

    deleteGraph: function(e) {
      var key = $(e.target).closest("a").attr("id"),
        self = this;
      // Ask user for permission
      this.collection.get(key).destroy({
        success: function() {
          self.render();
         }
      });
    },

    render: function() {
      this.collection.fetch({
        async: false
      });
      $(this.el).html(this.template.render({
        graphs: this.collection
      }));
      return this;
    }
  });

}());
