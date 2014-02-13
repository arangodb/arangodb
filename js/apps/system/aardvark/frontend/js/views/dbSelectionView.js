/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
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
      this.collection.fetch({
        async: false
      });
    },

    changeDatabase: function(e) {
//        var changeTo = $(".dbSelectionLink > option:selected").attr("id");
        var changeTo = $(".dbSelectionLink").attr("id");
      var url = this.collection.createDatabaseURL(changeTo);
      window.location.replace(url);
    },

    render: function(el) {
      this.$el = el;
      this.$el.html(this.template.render({
        list: this.collection,
        current: this.current.get("name")
      }));
      this.delegateEvents();
      return this.el;
    }
  });
}());
