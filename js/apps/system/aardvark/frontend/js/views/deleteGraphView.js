/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, $, _, window, templateEngine, arangoHelper*/

(function() {
  "use strict";

  window.DeleteGraphView = Backbone.View.extend({

    el: '#modalPlaceholder',

    template: templateEngine.createTemplate("deleteGraphView.ejs"),

    events: {
      "click #cancel": "hide",
      "hidden": "hidden",
      "click #confirmDelete": "deleteGraph"
    },

    deleteGraph: function() {
      var self = this;
      this.collection.get(this.name).destroy({
        success: function() {
          self.hide();
        },
        error: function(xhr, err) {
          var response = JSON.parse(err.responseText),
            msg = response.errorMessage;
          arangoHelper.arangoError(msg);
          self.hide();
        }
      });
    },

    hide: function() {
      $('#delete-graph').modal('hide');
    },

    hidden: function () {
      this.undelegateEvents();
      window.App.navigate("graphManagement", {trigger: true});
    },

    render: function(name) {
      if (!name) {
        window.App.navigate("graphManagement", {trigger: true});
      }
      this.name = name;
      $(this.el).html(this.template.render({
        name: name
      }));
      this.delegateEvents();
      $('#delete-graph').modal('show');
      return this;
    }

  });

}());

