/*jshint browser: true */
/*jshint unused: false */
/*global templateEngine, window, Backbone, $*/
(function() {
  "use strict";
  window.DBSelectionView = Backbone.View.extend({

    template: templateEngine.createTemplate("dbSelectionView.ejs"),

    events: {
      "click .dbSelectionLink": "changeDatabase"
    },

    initialize: function(opts) {
      this.current = opts.current;
    },

    changeDatabase: function(e) {
      var changeTo = $(e.currentTarget).closest(".dbSelectionLink.tab").attr("id");
      var url = this.collection.createDatabaseURL(changeTo);
      window.location.replace(url);
    },

    render: function(el) {
      this.$el = el;
      this.$el.html(this.template.render({
        list: this.collection.getDatabasesForUser(),
        current: this.current.get("name")
      }));
      this.delegateEvents();
      return this.el;
    }
  });
}());
